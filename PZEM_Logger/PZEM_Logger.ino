/*
 * PZEM_Logger.ino v4 - Hauptdatei
 * ===============================
 *
 * Korrekturen gegenüber v3:
 *  - statusLed.setOk() statt setState() in loop() (idempotent)
 *  - Logger toleriert PZEM-Einzelaussetzer (Schwellwert 3)
 *  - Buffer-Overflow bei SD-Ausfall sicher abgefangen (FIFO-Drop)
 *  - SD-Recovery wird automatisch versucht
 *  - JSON-Antwort benutzt statischen Stack-Buffer (kein Heap-Stress)
 *  - PROGMEM-HTML wird ohne Heap-Kopie versendet (send_P)
 *  - HTTP-Methoden explizit gemappt (HTTP_GET / HTTP_POST)
 */

#include "Config.h"
#include "StatusLed.h"
#include "Logger.h"
#include "WebPortal.h"

StatusLed  statusLed;
Logger     logger;
WebPortal  webPortal(logger);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== PZEM Logger v4 Start ===");

  // Pin-Modi
  pinMode(PIN_BUTTON, INPUT_PULLUP);   // momentan unbenutzt, aber sauber gesetzt
  pinMode(PIN_VSUPPLY, INPUT);
  analogReadResolution(12);

  statusLed.begin();
  statusLed.setOk(false);   // bis Initialisierung durch: Fehler-Blinken

  logger.begin();
  if (logger.sdOk()) {
    ensureLogHeader();
  }

  webPortal.begin();

  Serial.println("[Setup] fertig.");
}

void loop() {
  logger.pollIfDue();          // PZEM alle 500 ms
  logger.flushIfDue();         // SD alle 10 s
  webPortal.update();          // Webserver bedienen
  statusLed.setOk(logger.ok());// idempotent, kein Flackern
  statusLed.update();
}
