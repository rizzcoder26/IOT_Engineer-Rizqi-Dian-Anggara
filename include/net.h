#pragma once
#include <Arduino.h>

// Membuat net task (Wi-Fi + MQTT: telemetry, command, status/LWT). Mengembalikan handle task.
TaskHandle_t net_start();
