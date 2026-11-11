#pragma once
#include <Arduino.h>

// Membuat alarm task (state machine + LED + buzzer). Mengembalikan handle task.
TaskHandle_t alarm_start();

// Dipanggil button task saat tombol ACK ditekan (aman dipanggil dari task manapun).
void alarm_ack();
