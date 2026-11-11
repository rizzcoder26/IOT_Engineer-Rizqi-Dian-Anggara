#pragma once
#include <Arduino.h>

// Log ringan: [uptime ms] level tag pesan
#define LOG_(level, tag, fmt, ...) \
     Serial.printf("[%7lu] %c %-6s " fmt "\r\n", millis(), level, tag, ##__VA_ARGS__);

#define LOGI(tag, fmt, ...) LOG_('I', tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) LOG_('W', tag, fmt, ##__VA_ARGS__)
#define LOGE(tag, fmt, ...) LOG_('E', tag, fmt, ##__VA_ARGS__)
