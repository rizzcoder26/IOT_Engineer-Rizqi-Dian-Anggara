#include "sensors.h"
#include <DHT.h>
#include "config.h"
#include "log.h"
#include "state.h"

static const char* TAG = "sensor";

static DHT s_dht(PIN_DHT, DHT22);

static constexpr uint32_t TICKS_PER_SAMPLE = SENSOR_INTERVAL_MS / SENSOR_TICK_MS;

static int readLight() {
  int sum = 0;
  for (uint8_t i = 0; i < LDR_SAMPLES; i++) {
    sum += analogRead(PIN_LDR);
  }
  return sum / LDR_SAMPLES;
}

// DHT22 range: -40..80 C, 0..100 %RH. NaN = timeout / checksum error.
static bool dhtValid(float t, float h) {
  return !isnan(t) && !isnan(h) && t >= -40.0f && t <= 80.0f && h >= 0.0f && h <= 100.0f;
}

static void sensorsTask(void*) {
  SensorSnapshot s;          // salinan kerja lokal, dipublikasikan lewat state_setSensors()
  bool motionSeen = false;

  // DHT22 butuh ~2 s setelah power-up sebelum bacaan pertama.
  vTaskDelay(pdMS_TO_TICKS(2000));

  uint32_t tick = TICKS_PER_SAMPLE;   // paksa sampling lambat pada iterasi pertama
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    // Cepat (100 ms): PIR. Latch supaya gerakan singkat tidak terlewat di antara sampel 5 s.
    s.motionNow = digitalRead(PIN_PIR);
    motionSeen |= s.motionNow;

    // Lambat (5 s): DHT22 + LDR.
    if (++tick >= TICKS_PER_SAMPLE) {
      tick = 0;

      float t = s_dht.readTemperature();
      float h = s_dht.readHumidity();

      if (dhtValid(t, h)) {
        if (!s.dhtOk) {
          LOGI(TAG, "DHT22 OK");
        }
        s.tempC      = t;
        s.humidity   = h;
        s.dhtOk      = true;
        s.failCount  = 0;
      } else {
        if (s.failCount < 255) {
          s.failCount++;
        }
        LOGW(TAG, "DHT22 read failed (%u/%u)", s.failCount, DHT_FAIL_LIMIT);
        if (s.failCount >= DHT_FAIL_LIMIT && s.dhtOk) {
          LOGE(TAG, "DHT22 fault: data invalid");
        }
        if (s.failCount >= DHT_FAIL_LIMIT) {
          // Gagal berulang: jangan tampilkan/kirim nilai basi.
          s.dhtOk    = false;
          s.tempC    = NAN;
          s.humidity = NAN;
        }
        // Gagal sesekali: pertahankan nilai terakhir, coba lagi 5 s berikutnya.
      }

      s.lightRaw     = readLight();
      s.motionWindow = motionSeen;
      motionSeen     = false;
      s.seq++;

      LOGI(TAG, "T=%.1f H=%.1f L=%d PIR=%d ok=%d", s.tempC, s.humidity, s.lightRaw,
           s.motionWindow, s.dhtOk);
    }

    state_setSensors(s);
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_TICK_MS));
  }
}

TaskHandle_t sensors_start() {
  pinMode(PIN_PIR, INPUT);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);   // rentang ~0-3.3 V
  s_dht.begin();

  TaskHandle_t h = nullptr;
  xTaskCreatePinnedToCore(sensorsTask, "sensors", STACK_SENSORS, nullptr, PRIO_SENSORS, &h, CORE_APP);
  return h;
}
