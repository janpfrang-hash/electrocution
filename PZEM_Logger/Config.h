/*
 * Config.h - Zentrale Konfiguration v4 test
 */
#ifndef CONFIG_H
#define CONFIG_H

// ===== Pin-Belegung ESP32-WROOM =====
#define PIN_PZEM_RX     16
#define PIN_PZEM_TX     17
#define PIN_SD_CS       25
#define PIN_SD_MOSI     14
#define PIN_SD_CLK      27
#define PIN_SD_MISO     26
#define PIN_LED         32
#define PIN_BUTTON      33
#define PIN_VSUPPLY     35

// ===== Zeitintervalle (ms) =====
#define INTERVAL_PZEM_POLL_MS       500
#define INTERVAL_SD_FLUSH_MS      10000
#define INTERVAL_LED_OK_MS          500    // 1 Hz
#define INTERVAL_LED_ERR_MS         100    // 5 Hz

// ===== Fehler-Toleranz =====
// PZEM gilt erst nach so vielen aufeinanderfolgenden Lesefehlern als defekt.
#define PZEM_ERROR_THRESHOLD          3

// ===== WLAN =====
#define WIFI_AP_SSID        "PZEM_Logger"
#define WIFI_AP_PASSWORD    "logger1234"   // mind. 8 Zeichen

// Captive Portal / Hostname
#define WIFI_AP_HOSTNAME    "braun_PZEM"   // → http://braun_PZEM.net
#define DNS_PORT            53
// ===== SD-Karte =====
#define LOG_FILE_PATH    "/log.csv"
#define LOG_FILE_HEADER  "millis,voltage_V,power_W,pf"

// ===== RAM-Puffer =====
// 64 × 8 Byte = 512 Byte. 64 Samples × 0.5 s = 32 s Reserve.
#define RAM_BUFFER_SIZE  64

// ===== Webserver =====
#define HTTP_PORT        80
// Maximale Antwortgröße API (verhindert Heap-Stress)
#define API_BUFFER_SIZE  256

#endif
