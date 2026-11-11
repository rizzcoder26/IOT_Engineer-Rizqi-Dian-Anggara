#include "state.h"
#include <Preferences.h>
#include <freertos/semphr.h>
#include "config.h"
#include "log.h"

static const char* TAG = "state";

static SemaphoreHandle_t s_mtx = nullptr;
static SensorSnapshot    s_sensors;
static float             s_threshold = DEFAULT_THRESHOLD_C;
static AlarmState        s_alarm     = AlarmState::NORMAL;
static NetStatus         s_net;
static Preferences       s_prefs;

// RAII: ambil mutex di konstruktor, lepas di destruktor.
struct Lock {
  Lock()  { xSemaphoreTake(s_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(s_mtx); }
};

static bool thresholdValid(float v) {
  return isfinite(v) && v >= THRESHOLD_MIN_C && v <= THRESHOLD_MAX_C;
}

void state_init() {
  s_mtx = xSemaphoreCreateMutex();
  configASSERT(s_mtx != nullptr);

  // Ambang suhu persisten di NVS (bonus). Hanya dipakai dari net task setelah init.
  s_prefs.begin("smartroom", false);
  float saved = s_prefs.getFloat("thr", DEFAULT_THRESHOLD_C);
  if (thresholdValid(saved)) {
    s_threshold = saved;
  }
  LOGI(TAG, "threshold = %.1f C (NVS)", s_threshold);
}

void state_setSensors(const SensorSnapshot& s) {
  Lock l;
  s_sensors = s;
}

SensorSnapshot state_getSensors() {
  Lock l;
  return s_sensors;
}

float state_getThreshold() {
  Lock l;
  return s_threshold;
}

bool state_setThreshold(float v) {
  if (!thresholdValid(v)) {
    return false;
  }
  bool changed;
  {
    Lock l;
    changed = (s_threshold != v);
    s_threshold = v;
  }
  // Tulis flash di luar mutex agar task lain tidak tertahan; hanya jika berubah (hemat siklus tulis).
  if (changed) {
    s_prefs.putFloat("thr", v);
  }
  return true;
}

void state_setAlarm(AlarmState a) {
  Lock l;
  s_alarm = a;
}

AlarmState state_getAlarm() {
  Lock l;
  return s_alarm;
}

void state_setNet(bool wifi, bool mqtt) {
  Lock l;
  s_net.wifi = wifi;
  s_net.mqtt = mqtt;
}

NetStatus state_getNet() {
  Lock l;
  return s_net;
}
