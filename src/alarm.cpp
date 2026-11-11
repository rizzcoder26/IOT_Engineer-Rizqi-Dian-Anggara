#include "alarm.h"
#include "config.h"
#include "log.h"
#include "state.h"

static const char* TAG = "alarm";

static TaskHandle_t s_task = nullptr;

void alarm_ack() {
  if (s_task != nullptr) {
    xTaskNotifyGive(s_task);   // membangunkan alarm task segera, tanpa menunggu tick berikutnya
  }
}

static const char* name(AlarmState a) {
  switch (a) {
    case AlarmState::NORMAL: return "NORMAL";
    case AlarmState::ALARM:  return "ALARM";
    default:                 return "ACKED";
  }
}

// State machine:
//   NORMAL --(T > thr)-------------------> ALARM
//   ALARM  --(tombol ACK)----------------> ACKED
//   ALARM/ACKED --(T <= thr - histeresis)-> NORMAL   (re-arm: naik lagi melewati ambang -> ALARM)
// Jika DHT22 fault: state ditahan (tidak memicu alarm baru, tidak membatalkan alarm yang aktif).
static AlarmState step(AlarmState st, const SensorSnapshot& s, float thr, bool ackPressed) {
  const bool valid = s.dhtOk;
  const bool cooled = valid && (s.tempC <= thr - ALARM_HYSTERESIS_C);

  switch (st) {
    case AlarmState::NORMAL:
      if (valid && s.tempC > thr) return AlarmState::ALARM;
      break;
    case AlarmState::ALARM:
      if (ackPressed) return AlarmState::ACKED;
      if (cooled)     return AlarmState::NORMAL;
      break;
    case AlarmState::ACKED:
      if (cooled)     return AlarmState::NORMAL;
      break;
  }
  return st;
}

static void alarmTask(void*) {
  uint32_t phase = 0;   // dihitung per tick, dipakai untuk pola kedip/beep
  AlarmState st = AlarmState::NORMAL;

  for (;;) {
    // Tunggu notifikasi ACK, atau timeout = periode tick (jadi sekaligus pewaktu).
    bool ack = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ALARM_TICK_MS)) > 0;
    phase++;

    SensorSnapshot s = state_getSensors();
    float thr = state_getThreshold();

    AlarmState next = step(st, s, thr, ack);
    if (next != st) {
      LOGI(TAG, "%s -> %s (T=%.1f thr=%.1f)", name(st), name(next), s.tempC, thr);
      st = next;
      state_setAlarm(st);
    }

    const bool blink = (phase / 5) % 2;   // 5 tick x 100 ms = 500 ms on/off

    bool green = false, red = false, buzzer = false;
    switch (st) {
      case AlarmState::NORMAL:
        green = s.dhtOk;               // hijau hanya jika sensor suhu sesuai
        break;
      case AlarmState::ALARM:
        red = true;
        buzzer = blink;                // beep 1 Hz agar tidak membisingkan -> Adjustable
        break;
      case AlarmState::ACKED:
        red = ACKED_RED_BLINK ? blink : false;
        break;
    }

    digitalWrite(PIN_LED_G, green);
    digitalWrite(PIN_LED_R, red);
    digitalWrite(PIN_BUZZER, buzzer);
  }
}

TaskHandle_t alarm_start() {
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  xTaskCreatePinnedToCore(alarmTask, "alarm", STACK_ALARM, nullptr, PRIO_ALARM, &s_task, CORE_APP);
  return s_task;
}
