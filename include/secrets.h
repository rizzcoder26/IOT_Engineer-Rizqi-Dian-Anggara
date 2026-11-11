#pragma once

// Wokwi: SSID "Wokwi-GUEST", password kosong "".
#define WIFI_SSID   "Wokwi-GUEST"
#define WIFI_PASS   ""

// Broker publik untuk pengujian (port 1883, tanpa TLS).
#define MQTT_HOST   "broker.hivemq.com"
#define MQTT_PORT   1883

// Harus UNIK agar topik tidak bertabrakan dengan pengguna broker publik lain.
// Contoh: "room-rizqi-7f3a"
#define DEVICE_ID   "room-rizqi-1234"
