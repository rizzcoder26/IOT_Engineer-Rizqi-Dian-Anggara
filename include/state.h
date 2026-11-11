#pragma once
#include <Arduino.h>

// State bersama antar task. Semua akses lewat fungsi di bawah (dilindungi mutex),
// task tidak pernah menyentuh variabel global modul lain secara langsung.

enum class AlarmState : uint8_t { NORMAL, ALARM, ACKED };

struct SensorSnapshot {
  float    tempC        = NAN;
  float    humidity     = NAN;
  int      lightRaw     = 0;
  bool     motionNow    = false;  // kondisi PIR terkini (untuk OLED)
  bool     motionWindow = false;  // ada gerakan sejak sampel 5 s sebelumnya (untuk telemetri)
  bool     dhtOk        = false;  // false sebelum bacaan pertama & setelah DHT_FAIL_LIMIT gagal beruntun
  uint8_t  failCount    = 0;
  uint32_t seq          = 0;      // naik 1 setiap siklus sampling 5 s
};

struct NetStatus {
  bool wifi = false;
  bool mqtt = false;
};

void  state_init();   // buat mutex + muat ambang dari NVS

void           state_setSensors(const SensorSnapshot& s);
SensorSnapshot state_getSensors();

float state_getThreshold();
bool  state_setThreshold(float v);   // false jika di luar rentang; menyimpan ke NVS bila berubah

void       state_setAlarm(AlarmState a);
AlarmState state_getAlarm();

void      state_setNet(bool wifi, bool mqtt);
NetStatus state_getNet();
