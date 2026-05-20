/*
 * Logger.h v4
 */
#ifndef LOGGER_H
#define LOGGER_H
 
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <PZEM004Tv30.h>
#include "Config.h"
 
struct Sample {
  uint32_t millis_ts;
  float    voltage_V;
  float    power_W;
  float    pf;
};
 
class Logger {
public:
  Logger()
    : _pzem(Serial2, PIN_PZEM_RX, PIN_PZEM_TX),
      _sdSPI(VSPI),
      _bufferCount(0),
      _droppedSamples(0),
      _lastPollMs(0),
      _lastFlushMs(0),
      _lastSdRetryMs(0),
      _lastVoltage(NAN),
      _lastPower(NAN),
      _lastPf(NAN),
      _pzemErrorCount(0),
      _sdOk(false),
      _pollIntervalMs(INTERVAL_PZEM_POLL_MS)
  {}
 
  bool begin() {
    _sdSPI.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    _sdOk = initSDCard();
    _pzemErrorCount = 0;
    Serial.printf("[Logger] SD=%s\n", _sdOk ? "OK" : "FEHLER");
    return _sdOk;
  }
 
  // Set poll interval at runtime (called from WebPortal settings handler)
  void setPollInterval(uint32_t ms) { if (ms > 0) _pollIntervalMs = ms; }
  uint32_t getPollInterval() const  { return _pollIntervalMs; }
 
  void pollIfDue() {
    uint32_t now = millis();
    if (now - _lastPollMs < _pollIntervalMs) return;
    _lastPollMs = now;
 
    float V = _pzem.voltage();
    float P = _pzem.power();
    float PF = _pzem.pf();
    
    if (isnan(V) || isnan(P) || isnan(PF)) {
      if (_pzemErrorCount < 255) _pzemErrorCount++;
      if (_pzemErrorCount == PZEM_ERROR_THRESHOLD) {
        Serial.println("[Logger] PZEM dauerhaft fehlerhaft");
      }
      return;   
    }
 
    if (_pzemErrorCount >= PZEM_ERROR_THRESHOLD) {
      Serial.println("[Logger] PZEM wieder erreichbar");
    }
    _pzemErrorCount = 0;
    _lastVoltage = V;
    _lastPower = P;
    _lastPf = PF;
    pushSample({ now, V, P, PF });
  }
 
  void flushIfDue() {
    uint32_t now = millis();
    if (now - _lastFlushMs < INTERVAL_SD_FLUSH_MS) return;
    _lastFlushMs = now;
 
    if (!_sdOk) {
      tryRecoverSD();
      return;
    }
    flushToSD();
  }
 
  bool flushToSD() {
    if (!_sdOk || _bufferCount == 0) return _sdOk;
 
    File f = SD.open(LOG_FILE_PATH, FILE_APPEND);
    if (!f) {
      Serial.println("[Logger] SD-Open fehlgeschlagen, markiere SD als defekt");
      _sdOk = false;
      return false;
    }
 
    for (size_t i = 0; i < _bufferCount; i++) {
      f.printf("%lu,%.1f,%.1f,%.2f\n",
               (unsigned long)_buffer[i].millis_ts,
               _buffer[i].voltage_V,
               _buffer[i].power_W,
               _buffer[i].pf);
    }
    f.flush();
    f.close();
 
    Serial.printf("[Logger] %u Samples auf SD geschrieben\n",
                  (unsigned)_bufferCount);
    _bufferCount = 0;
    return true;
  }
 
  bool resetSDFile() {
    if (!_sdOk) return false;
    _bufferCount = 0;
    if (SD.exists(LOG_FILE_PATH)) {
      if (!SD.remove(LOG_FILE_PATH)) {
        Serial.println("[Logger] SD.remove fehlgeschlagen");
        return false;
      }
    }
    File f = SD.open(LOG_FILE_PATH, FILE_WRITE);
    if (!f) return false;
    f.println(LOG_FILE_HEADER);
    f.close();
    Serial.println("[Logger] Log-Datei zurückgesetzt");
    return true;
  }
 
  bool   pzemOk() const         { return _pzemErrorCount < PZEM_ERROR_THRESHOLD; }
  bool   sdOk() const           { return _sdOk; }
  bool   ok() const             { return pzemOk() && _sdOk; }
  float  getLastVoltage() const { return _lastVoltage; }
  float  getLastPower() const   { return _lastPower; }  
  float  getLastPf() const      { return _lastPf; }
  size_t getBufferCount() const { return _bufferCount; }
  uint32_t getDroppedSamples() const { return _droppedSamples; }
 
  File openLogFileForRead() {
    if (!_sdOk) return File();
    return SD.open(LOG_FILE_PATH, FILE_READ);
  }
 
private:
  PZEM004Tv30 _pzem;
  SPIClass    _sdSPI;
  Sample      _buffer[RAM_BUFFER_SIZE];
  size_t      _bufferCount;
  uint32_t    _droppedSamples;
  uint32_t    _lastPollMs;
  uint32_t    _lastFlushMs;
  uint32_t    _lastSdRetryMs;
  float       _lastVoltage;
  float       _lastPower;
  float       _lastPf;
  uint8_t     _pzemErrorCount;
  bool        _sdOk;
  uint32_t    _pollIntervalMs;   // runtime-adjustable, default 500 ms
 
  void pushSample(const Sample& s) {
    if (_bufferCount < RAM_BUFFER_SIZE) {
      _buffer[_bufferCount++] = s;
      return;
    }
    if (_sdOk && flushToSD()) {
      _buffer[_bufferCount++] = s;
      return;
    }
    memmove(&_buffer[0], &_buffer[1],
            (RAM_BUFFER_SIZE - 1) * sizeof(Sample));
    _buffer[RAM_BUFFER_SIZE - 1] = s;
    _droppedSamples++;
    if (_droppedSamples == 1 || _droppedSamples % 100 == 0) {
      Serial.printf("[Logger] WARNUNG: Sample verworfen (gesamt: %lu)\n",
                    (unsigned long)_droppedSamples);
    }
  }
 
  bool initSDCard() {
    return SD.begin(PIN_SD_CS, _sdSPI, 4000000);
  }
 
  void tryRecoverSD() {
    uint32_t now = millis();
    if (now - _lastSdRetryMs < 30000) return;
    _lastSdRetryMs = now;
 
    Serial.println("[Logger] Versuche SD-Karte neu zu initialisieren...");
    SD.end();
    delay(50);
    _sdOk = initSDCard();
    if (_sdOk) {
      if (!SD.exists(LOG_FILE_PATH)) {
        File f = SD.open(LOG_FILE_PATH, FILE_WRITE);
        if (f) { f.println(LOG_FILE_HEADER); f.close(); }
      }
      Serial.println("[Logger] SD wieder verfügbar");
    }
  }
};
 
inline void ensureLogHeader() {
  if (!SD.exists(LOG_FILE_PATH)) {
    File f = SD.open(LOG_FILE_PATH, FILE_WRITE);
    if (f) {
      f.println(LOG_FILE_HEADER);
      f.close();
    }
  }
}
 
#endif
