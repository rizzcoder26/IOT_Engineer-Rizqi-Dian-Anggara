#include <Arduino.h>
#include "alarm.h"
#include "button.h"
#include "config.h"
#include "display.h"
#include "log.h"
#include "net.h"
#include "sensors.h"
#include "state.h"

static const char* TAG = "main";

struct NamedTask {
  const char*  name;
  TaskHandle_t handle;
};
static NamedTask s_tasks[5];

// Task pengawas berprioritas rendah: mencetak sisa stack (high-water mark) dan heap.
// Berguna saat debugging: kalau sisa stack mendekati 0, naikkan STACK_* di config.h.
static void monitorTask(void*) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(MONITOR_INTERVAL_MS));
    LOGI(TAG, "heap free=%u min=%u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
    for (const NamedTask& t : s_tasks) {
      if (t.handle != nullptr) {
        LOGI(TAG, "stack %-8s free=%u B", t.name, (unsigned)uxTaskGetStackHighWaterMark(t.handle));
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  LOGI(TAG, "Smart Room Monitor boot");

  state_init();

  s_tasks[0] = {"button",  button_start()};
  s_tasks[1] = {"alarm",   alarm_start()};
  s_tasks[2] = {"sensors", sensors_start()};
  s_tasks[3] = {"display", display_start()};
  s_tasks[4] = {"net",     net_start()};

  xTaskCreatePinnedToCore(monitorTask, "monitor", STACK_MONITOR, nullptr, PRIO_MONITOR, nullptr, CORE_APP);
}

// Semua kerja dilakukan oleh task; loop() bawaan Arduino tidak dibutuhkan.
void loop() {
  vTaskDelete(nullptr);
}
