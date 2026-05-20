/*
 * WebPortal.h v4
 * --------------
 * Verbesserungen:
 *  - Statisch allokierter Char-Buffer für API-JSON (kein Heap-Stress)
 *  - PROGMEM HTML wird ohne String-Kopie gesendet (send_P)
 *  - Konstante HTML-Strings ohne Replace (jede Seite hat ihren Buffer)
 *  - Fehler-Pfade konsistent als 4xx/5xx
 */
#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>        // ← NEU
#include <ESPmDNS.h>          // ← NEU
#include "Config.h"
#include "Logger.h"

class WebPortal {
public:
  WebPortal(Logger& logger);
  bool begin();
  void update();

private:
  Logger&   _logger;
  WebServer _server;
  DNSServer _dns;             // ← NEU

  // Handler
  void handleRoot();
  void handleApiLive();
  void handleDownload();
  void handleReset();
  void handleSettings();
  void handleReadme();
  void handleCaptivePortal(); // ← NEU
  void handleNotFound();
};

#endif