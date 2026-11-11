#pragma once
#include <Arduino.h>

// Pin map
constexpr uint8_t PIN_DHT    = 4;
constexpr uint8_t PIN_LDR    = 34;   // ADC1, input only
constexpr uint8_t PIN_PIR    = 27;
constexpr uint8_t PIN_LED_G  = 25;
constexpr uint8_t PIN_LED_R  = 26;
constexpr uint8_t PIN_BUZZER = 33;
constexpr uint8_t PIN_BTN    = 14;   // INPUT_PULLUP, tombol ke GND
constexpr uint8_t PIN_SDA    = 21;
constexpr uint8_t PIN_SCL    = 22;

// Sensors
constexpr uint32_t SENSOR_TICK_MS     = 100;    // PIR disampling tiap 100 ms
constexpr uint32_t SENSOR_INTERVAL_MS = 5000;   // DHT22 + LDR + telemetri tiap 5 s
constexpr uint8_t  DHT_FAIL_LIMIT     = 3;      // gagal beruntun sebelum dianggap fault
constexpr uint8_t  LDR_SAMPLES        = 4;      // rata-rata untuk meredam noise ADC

// Alarm
constexpr float    DEFAULT_THRESHOLD_C = 30.0f;
constexpr float    THRESHOLD_MIN_C     = 10.0f; // batas valid untuk perintah MQTT
constexpr float    THRESHOLD_MAX_C     = 60.0f;
constexpr float    ALARM_HYSTERESIS_C  = 0.5f;  // alarm re-arm saat suhu <= ambang - histeresis
constexpr uint32_t ALARM_TICK_MS       = 100;
constexpr bool     ACKED_RED_BLINK     = true;  // true: LED merah berkedip pelan setelah ACK

// Button
constexpr uint32_t BTN_POLL_MS     = 10;
constexpr uint8_t  BTN_DEBOUNCE_N  = 3;         // 3 x 10 ms = 30 ms stabil

// Display
constexpr uint8_t  OLED_ADDR       = 0x3C;
constexpr uint32_t DISPLAY_TICK_MS = 500;

// Network
constexpr uint32_t NET_TICK_MS        = 50;
constexpr uint32_t WIFI_RETRY_MIN_MS  = 8000;
constexpr uint32_t WIFI_RETRY_MAX_MS  = 60000;
constexpr uint32_t MQTT_RETRY_MIN_MS  = 2000;
constexpr uint32_t MQTT_RETRY_MAX_MS  = 30000;
constexpr uint16_t MQTT_SOCKET_TIMEOUT_S = 4;
constexpr uint16_t MQTT_KEEPALIVE_S      = 30;

// RTOS
// Prioritas: makin besar makin tinggi. Idle task = 0.
constexpr UBaseType_t PRIO_BUTTON  = 4;   // responsif terhadap ACK
constexpr UBaseType_t PRIO_ALARM   = 3;
constexpr UBaseType_t PRIO_SENSORS = 2;
constexpr UBaseType_t PRIO_NET     = 2;
constexpr UBaseType_t PRIO_DISPLAY = 1;
constexpr UBaseType_t PRIO_MONITOR = 1;

// Ukuran stack dalam BYTE (xTaskCreate di Arduino-ESP32).
constexpr uint32_t STACK_BUTTON  = 2048;
constexpr uint32_t STACK_ALARM   = 3072;
constexpr uint32_t STACK_SENSORS = 3072;
constexpr uint32_t STACK_DISPLAY = 4096;
constexpr uint32_t STACK_NET     = 8192;
constexpr uint32_t STACK_MONITOR = 3072;

// Core 0 menjalankan WiFi stack, core 1 menjalankan task aplikasi lokal.
constexpr BaseType_t CORE_NET = 0;
constexpr BaseType_t CORE_APP = 1;

constexpr uint32_t MONITOR_INTERVAL_MS = 30000; // log stack watermark + heap
