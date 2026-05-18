/*
 * StatusLed.h v4
 * --------------
 * Verbesserungen ggü. v3:
 *  - setState() setzt _lastToggle NUR bei echtem Statuswechsel zurück
 *    (Aufrufer kann gefahrlos jeden Loop-Durchlauf setState() aufrufen)
 *  - Hilfsmethode setOk(bool) für klareren Aufruf
 */
#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include "Config.h"

enum LedState {
  LED_OK,
  LED_ERROR
};

class StatusLed {
public:
  void begin() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    _state      = LED_OK;
    _lastToggle = millis();
    _ledOn      = false;
  }

  // Setzt den Zustand. Nur bei Wechsel wird Blink-Phase resettet.
  void setState(LedState s) {
    if (s == _state) return;
    _state      = s;
    _lastToggle = millis();
    _ledOn      = true;            // sofort sichtbarer Wechsel
    digitalWrite(PIN_LED, HIGH);
  }

  void setOk(bool ok) { setState(ok ? LED_OK : LED_ERROR); }

  LedState getState() const { return _state; }

  void update() {
    uint32_t interval = (_state == LED_OK) ? INTERVAL_LED_OK_MS
                                           : INTERVAL_LED_ERR_MS;
    uint32_t now = millis();
    if (now - _lastToggle >= interval) {
      _lastToggle = now;
      _ledOn = !_ledOn;
      digitalWrite(PIN_LED, _ledOn ? HIGH : LOW);
    }
  }

private:
  LedState _state;
  uint32_t _lastToggle;
  bool     _ledOn;
};

#endif
