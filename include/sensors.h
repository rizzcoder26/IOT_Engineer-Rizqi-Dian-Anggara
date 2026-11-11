#pragma once
#include <Arduino.h>

// Membuat sensor task (DHT22, LDR, PIR). Mengembalikan handle task.
TaskHandle_t sensors_start();
