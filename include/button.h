#pragma once
#include <Arduino.h>

// Membuat button task (debounce + kirim event ACK ke alarm). Mengembalikan handle task.
TaskHandle_t button_start();
