#include "button.h"
#include "alarm.h"
#include "config.h"
#include "log.h"

static const char* TAG = "button";

// Debounce dengan penghitung: level baru harus stabil BTN_DEBOUNCE_N polling berturut-turut
// sebelum diterima. Event hanya pada tepi turun (HIGH -> LOW), tahan tombol tidak mengulang.
static void buttonTask(void*) {
  bool    stable = HIGH;      // Pullup 
  uint8_t count  = 0;
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(BTN_POLL_MS));

    bool raw = digitalRead(PIN_BTN);
    if (raw == stable) {
      count = 0;
      continue;
    }
    if (++count >= BTN_DEBOUNCE_N) {
      stable = raw;
      count = 0;
      if (stable == LOW) {
        LOGI(TAG, "ACK pressed");
        alarm_ack();
      }
    }
  }
}

TaskHandle_t button_start() {
  pinMode(PIN_BTN, INPUT_PULLUP);
  TaskHandle_t h = nullptr;
  xTaskCreatePinnedToCore(buttonTask, "button", STACK_BUTTON, nullptr, PRIO_BUTTON, &h, CORE_APP);
  return h;
}
