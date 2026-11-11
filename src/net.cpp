#include "net.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_timer.h>
#include "config.h"
#include "log.h"
#include "secrets.h"   // WIFI_SSID, WIFI_PASS, MQTT_HOST, MQTT_PORT, DEVICE_ID
#include "state.h"

static const char* TAG = "net";

static WiFiClient   s_wifi;
static PubSubClient s_mqtt(s_wifi);

static char s_topicTelemetry[96];
static char s_topicCommand[96];
static char s_topicStatus[96];
static char s_clientId[48];

static uint32_t s_nextWifiAttempt = 0;
static uint32_t s_wifiBackoff     = WIFI_RETRY_MIN_MS;
static uint32_t s_nextMqttAttempt = 0;
static uint32_t s_mqttBackoff     = MQTT_RETRY_MIN_MS;
static bool     s_wifiWasUp       = false;
static bool     s_mqttWasUp       = false;
static uint32_t s_lastSeq         = 0;
static bool     s_publishNow      = false;

static inline bool due(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;   // aman terhadap overflow millis()
}

// MQTT command
// Payload: {"threshold": 32.5}
static void onMessage(char* topic, byte* payload, unsigned int len) {
  LOGI(TAG, "RX %s (%u bytes)", topic, len);
  if (len == 0 || len > 128) {
    LOGW(TAG, "command ignored: bad length");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, (const char*)payload, len);
  if (err) {
    LOGW(TAG, "command ignored: JSON error %s", err.c_str());
    return;
  }
  if (!doc["threshold"].is<float>()) {
    LOGW(TAG, "command ignored: no numeric \"threshold\"");
    return;
  }

  float v = doc["threshold"].as<float>();
  if (!state_setThreshold(v)) {
    LOGW(TAG, "command ignored: threshold %.1f out of range [%.0f..%.0f]", v,
         THRESHOLD_MIN_C, THRESHOLD_MAX_C);
    return;
  }
  LOGI(TAG, "threshold set to %.1f C", v);
  s_publishNow = true;   // kirim telemetri segera agar server melihat ambang baru
}

// Wi-Fi
static void serviceWifi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!s_wifiWasUp) {
      s_wifiWasUp   = true;
      s_wifiBackoff = WIFI_RETRY_MIN_MS;
      LOGI(TAG, "WiFi connected, IP %s", WiFi.localIP().toString().c_str());
    }
    return;
  }

  if (s_wifiWasUp) {
    s_wifiWasUp = false;
    LOGW(TAG, "WiFi lost");
  }

  // Reconnect dengan exponential backoff, tidak memblokir.
  if (due(now, s_nextWifiAttempt)) {
    LOGI(TAG, "WiFi connecting to \"%s\" ...", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    s_nextWifiAttempt = now + s_wifiBackoff;
    s_wifiBackoff = min<uint32_t>(s_wifiBackoff * 2, WIFI_RETRY_MAX_MS);
  }
}

//  MQTT
static void serviceMqtt(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (s_mqtt.connected()) {
    if (!s_mqttWasUp) {
      s_mqttWasUp   = true;
      s_mqttBackoff = MQTT_RETRY_MIN_MS;
    }
    return;
  }

  if (s_mqttWasUp) {
    s_mqttWasUp = false;
    LOGW(TAG, "MQTT lost (state %d)", s_mqtt.state());
  }

  if (!due(now, s_nextMqttAttempt)) {
    return;
  }

  LOGI(TAG, "MQTT connecting to %s:%d as %s", MQTT_HOST, MQTT_PORT, s_clientId);
  // Last Will: broker mempublikasikan "offline" (retained) jika perangkat putus tanpa pamit.
  bool ok = s_mqtt.connect(s_clientId, nullptr, nullptr, s_topicStatus, 1, true, "offline");
  if (ok) {
    s_mqtt.publish(s_topicStatus, "online", true);   // retained
    s_mqtt.subscribe(s_topicCommand);
    s_publishNow = true;                             // laporkan kondisi awal segera
    LOGI(TAG, "MQTT connected, sub %s", s_topicCommand);
  } else {
    s_nextMqttAttempt = now + s_mqttBackoff;
    LOGW(TAG, "MQTT connect failed (state %d), retry in %lu ms", s_mqtt.state(),
         (unsigned long)s_mqttBackoff);
    s_mqttBackoff = min<uint32_t>(s_mqttBackoff * 2, MQTT_RETRY_MAX_MS);
  }
}

// Telemetry
static float round1(float v) {
  return roundf(v * 10.0f) / 10.0f;
}

static void publishTelemetry(const SensorSnapshot& s) {
  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  if (s.dhtOk) {
    doc["temp_c"]   = round1(s.tempC);
    doc["humidity"] = round1(s.humidity);
  } else {
    doc["temp_c"]   = nullptr;   // sensor fault: kirim null, bukan angka basi
    doc["humidity"] = nullptr;
  }
  doc["light_raw"]  = s.lightRaw;
  doc["motion"]     = s.motionWindow;
  doc["alarm"]      = (state_getAlarm() == AlarmState::ALARM);
  doc["uptime_s"]   = (uint32_t)(esp_timer_get_time() / 1000000ULL);
  doc["threshold"]  = state_getThreshold();
  doc["sensor_ok"]  = s.dhtOk;

  char buf[256];
  size_t n = serializeJson(doc, buf, sizeof buf);
  if (n == 0 || n >= sizeof buf) {
    LOGE(TAG, "telemetry JSON too large");
    return;
  }
  if (s_mqtt.publish(s_topicTelemetry, buf)) {
    LOGI(TAG, "TX %s", buf);
  } else {
    LOGW(TAG, "telemetry publish failed");
  }
}

//  Task
static void netTask(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  bool lastWifi = false, lastMqtt = false;

  for (;;) {
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(NET_TICK_MS));
    const uint32_t now = millis();

    serviceWifi(now);
    serviceMqtt(now);
    if (s_mqtt.connected()) {
      s_mqtt.loop();
    }

    // Bagikan status koneksi ke OLED hanya saat berubah.
    bool wifi = (WiFi.status() == WL_CONNECTED);
    bool mqtt = s_mqtt.connected();
    if (wifi != lastWifi || mqtt != lastMqtt) {
      lastWifi = wifi;
      lastMqtt = mqtt;
      state_setNet(wifi, mqtt);
    }

    // Telemetri dipicu oleh sampel baru dari sensor task (seq berubah) -> tepat 1x per 5 s,
    // tidak pernah mengirim data basi dua kali. Jika MQTT sedang putus, sampel itu dilewati.
    SensorSnapshot s = state_getSensors();
    bool fresh = (s.seq != s_lastSeq);
    if (fresh) {
      s_lastSeq = s.seq;
    }
    if (s_mqtt.connected() && (fresh || s_publishNow) && s.seq != 0) {
      s_publishNow = false;
      publishTelemetry(s);
    }
  }
}

TaskHandle_t net_start() {
  snprintf(s_topicTelemetry, sizeof s_topicTelemetry, "smartroom/%s/telemetry", DEVICE_ID);
  snprintf(s_topicCommand,   sizeof s_topicCommand,   "smartroom/%s/command",   DEVICE_ID);
  snprintf(s_topicStatus,    sizeof s_topicStatus,    "smartroom/%s/status",    DEVICE_ID);
  // Client ID harus unik di broker publik; tambahkan potongan MAC.
  snprintf(s_clientId, sizeof s_clientId, "%s-%04X", DEVICE_ID, (uint16_t)(ESP.getEfuseMac() >> 32));

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  s_mqtt.setServer(MQTT_HOST, MQTT_PORT);
  s_mqtt.setCallback(onMessage);
  s_mqtt.setBufferSize(512);
  s_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  s_mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);

  TaskHandle_t h = nullptr;
  xTaskCreatePinnedToCore(netTask, "net", STACK_NET, nullptr, PRIO_NET, &h, CORE_NET);
  return h;
}
