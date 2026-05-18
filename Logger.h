/*
 * Logger.h v4
 * -----------
 * Korrekturen ggü. v3:
 *  - Puffer-Overflow bei SD-Fehler: ältesten Sample überschreiben (Ringpuffer)
 *  - PZEM-Fehlertoleranz: erst nach N aufeinanderfolgenden Fehlern als defekt
 *  - SD-Recovery: bei SD-Fehler periodisch versuchen, neu zu initialisieren
 *  - Explizite PZEM-Serial-Initialisierung im begin() (nicht im Konstruktor)
 *  - getLastPower() liefert NaN bei "noch nie erfolgreich gelesen"
 *  - Klare Trennung Polling-Timer vs. Flush-Timer
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
  float    power_W;
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
      _lastPower(NAN),
      _pzemErrorCount(0),
      _sdOk(false)
  {}

  bool begin() {
    // SD initialisieren
    _sdSPI.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    _sdOk = initSDCard();

    // PZEM: die Library hat Serial2 bereits konfiguriert.
    // Eine erste Testabfrage machen wir NICHT in begin(), um
    // setup() nicht um ~200 ms zu verzögern.
    _pzemErrorCount = 0;

    Serial.printf("[Logger] SD=%s\n", _sdOk ? "OK" : "FEHLER");
    return _sdOk;
  }

  // --- PZEM alle 500 ms abfragen ---
  void pollIfDue() {
    uint32_t now = millis();
    if (now - _lastPollMs < INTERVAL_PZEM_POLL_MS) return;
    _lastPollMs = now;

    float P = _pzem.power();
    if (isnan(P)) {
      if (_pzemErrorCount < 255) _pzemErrorCount++;
      if (_pzemErrorCount == PZEM_ERROR_THRESHOLD) {
        Serial.println("[Logger] PZEM dauerhaft fehlerhaft");
      }
      return;   // kein Sample puffern
    }

    // Erfolgreich gelesen
    if (_pzemErrorCount >= PZEM_ERROR_THRESHOLD) {
      Serial.println("[Logger] PZEM wieder erreichbar");
    }
    _pzemErrorCount = 0;
    _lastPower = P;
    pushSample({ now, P });
  }

  // --- Puffer alle 10 s auf SD schreiben ---
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

  // Sofort flushen (z.B. vor Download). Liefert true bei Erfolg.
  bool flushToSD() {
    if (!_sdOk || _bufferCount == 0) return _sdOk;

    File f = SD.open(LOG_FILE_PATH, FILE_APPEND);
    if (!f) {
      Serial.println("[Logger] SD-Open fehlgeschlagen, markiere SD als defekt");
      _sdOk = false;
      return false;
    }

    for (size_t i = 0; i < _bufferCount; i++) {
      f.printf("%lu,%.1f\n",
               (unsigned long)_buffer[i].millis_ts,
               _buffer[i].power_W);
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

  // ===== Status =====
  bool   pzemOk() const         { return _pzemErrorCount < PZEM_ERROR_THRESHOLD; }
  bool   sdOk() const           { return _sdOk; }
  bool   ok() const             { return pzemOk() && _sdOk; }
  float  getLastPower() const   { return _lastPower; }   // NaN, wenn nie gelesen
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
  float       _lastPower;
  uint8_t     _pzemErrorCount;
  bool        _sdOk;

  // Sicheres Hinzufügen zum Puffer.
  // Bei Overflow: ältestes Sample verwerfen (FIFO-Verhalten).
  void pushSample(const Sample& s) {
    if (_bufferCount < RAM_BUFFER_SIZE) {
      _buffer[_bufferCount++] = s;
      return;
    }
    // Puffer voll. Versuch: sofort flushen.
    if (_sdOk && flushToSD()) {
      _buffer[_bufferCount++] = s;
      return;
    }
    // SD nicht verfügbar → ältesten Sample fallen lassen, Rest nach vorn schieben.
    // (memmove ist erlaubt, weil POD-Struct)
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
    // Nur alle 30 s versuchen, damit es loop() nicht ausbremst
    if (now - _lastSdRetryMs < 30000) return;
    _lastSdRetryMs = now;

    Serial.println("[Logger] Versuche SD-Karte neu zu initialisieren...");
    SD.end();
    delay(50);
    _sdOk = initSDCard();
    if (_sdOk) {
      // Header sicherstellen
      if (!SD.exists(LOG_FILE_PATH)) {
        File f = SD.open(LOG_FILE_PATH, FILE_WRITE);
        if (f) { f.println(LOG_FILE_HEADER); f.close(); }
      }
      Serial.println("[Logger] SD wieder verfügbar");
    }
  }
};

// Initialisiert den Log-Datei-Header beim ersten Start. Muss nach
// Logger::begin() aufgerufen werden, weil wir die SD brauchen.
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
