#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "log.h"
#include "state.h"

static const char* TAG = "display";

static Adafruit_SSD1306 s_oled(128, 64, &Wire, -1);

static void line(uint8_t row, const char* text) {
  s_oled.setCursor(0, row * 8);
  s_oled.print(text);
}

static void render() {
  SensorSnapshot s = state_getSensors();
  float      thr   = state_getThreshold();
  AlarmState a     = state_getAlarm();
  NetStatus  n     = state_getNet();

  char buf[24];
  s_oled.clearDisplay();

  line(0, "SMART ROOM MONITOR");

  if (s.dhtOk) {
    snprintf(buf, sizeof buf, "Temp : %.1f C", s.tempC);
    line(1, buf);
    snprintf(buf, sizeof buf, "Humi : %.1f %%", s.humidity);
    line(2, buf);
  } else {
    line(1, "Temp : ERR");
    line(2, "Humi : ERR");
  }

  snprintf(buf, sizeof buf, "Light: %d", s.lightRaw);
  line(3, buf);
  snprintf(buf, sizeof buf, "Motion: %s", s.motionNow ? "YES" : "no");
  line(4, buf);
  snprintf(buf, sizeof buf, "Thr  : %.1f C", thr);
  line(5, buf);

  const char* as = (a == AlarmState::ALARM) ? "ALARM!" : (a == AlarmState::ACKED) ? "ACK" : "NORMAL";
  snprintf(buf, sizeof buf, "Alarm: %s", as);
  line(6, buf);

  snprintf(buf, sizeof buf, "WiFi:%s MQTT:%s", n.wifi ? "OK" : "--", n.mqtt ? "OK" : "--");
  line(7, buf);

  s_oled.display();
}

static void displayTask(void*) {
  // Mekanisme cek OLED
  while (!s_oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    LOGE(TAG, "SSD1306 not found at 0x%02X, retry in 5 s", OLED_ADDR);
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
  LOGI(TAG, "SSD1306 ready");
  s_oled.setTextSize(1);
  s_oled.setTextColor(SSD1306_WHITE);

  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    render();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(DISPLAY_TICK_MS));
  }
}

TaskHandle_t display_start() {
  Wire.begin(PIN_SDA, PIN_SCL);
  TaskHandle_t h = nullptr;
  xTaskCreatePinnedToCore(displayTask, "display", STACK_DISPLAY, nullptr, PRIO_DISPLAY, &h, CORE_APP);
  return h;
}
