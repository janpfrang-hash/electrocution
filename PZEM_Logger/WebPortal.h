/*
 * WebPortal.h v5
 * --------------
 * Änderungen gegenüber v4:
 *  - Statisch allokierter Char-Buffer für API-JSON (kein Heap-Stress)
 *  - PROGMEM HTML wird ohne String-Kopie gesendet (send_P)
 *  - Fehler-Pfade konsistent als 4xx/5xx
 *  - Settings-Seite: Poll-Rate per POST /api/settings speicherbar
 *  - /api/settings GET liefert aktuelle Einstellungen als JSON
 */
#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
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
  DNSServer _dns;

  // Handler
  void handleRoot();
  void handleApiLive();
  void handleApiSettings();     // GET  /api/settings
  void handleApiSettingsSave(); // POST /api/settings
  void handleDownload();
  void handleReset();
  void handleSettings();
  void handleReadme();
  void handleCaptivePortal();
  void handleNotFound();
};

#endif
