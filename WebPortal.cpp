/*
 * WebPortal.cpp v4 - heap-effizient
 */
#include "WebPortal.h"

// Hauptseite in PROGMEM, ohne Platzhalter (alles via fetch /api/live)
static const char PAGE_INDEX[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PZEM Logger</title>
<style>
  body { font-family: sans-serif; max-width: 600px; margin: 1em auto;
         padding: 1em; background: #f5f5f5; color: #222; }
  h1 { color: #007acc; }
  .card { background: white; border-radius: 8px; padding: 1em;
          margin-bottom: 1em; box-shadow: 0 1px 3px rgba(0,0,0,.1); }
  .live { font-size: 2.5em; font-weight: bold; color: #007acc;
          text-align: center; margin: .3em 0; }
  .label { text-align: center; color: #666; }
  .btn-row { display: grid; grid-template-columns: 1fr 1fr; gap: .6em; }
  button { padding: .9em; font-size: 1em; border: none; border-radius: 6px;
           background: #007acc; color: white; cursor: pointer; }
  button:hover { background: #005f99; }
  button.danger { background: #d33; }
  button.danger:hover { background: #a22; }
  button.muted { background: #888; }
  .status { font-size: .9em; color: #666; margin-top: .8em; }
  .status span { display: inline-block; padding: .1em .5em;
                 border-radius: 3px; margin-right: .3em; }
  .ok  { background: #cfc; color: #060; }
  .err { background: #fcc; color: #800; }
</style>
</head><body>

<h1>PZEM-004T Logger</h1>

<div class="card">
  <div class="label">Aktuelle Leistung</div>
  <div class="live"><span id="power">—</span> W</div>
  <div class="status">
    PZEM: <span id="pzem-status" class="ok">?</span>
    SD:   <span id="sd-status"   class="ok">?</span>
    Puffer: <span id="buf">0</span>
    | Verworfen: <span id="drop">0</span>
    | Uptime: <span id="uptime">0</span> s
  </div>
</div>

<div class="card">
  <div class="btn-row">
    <button onclick="location.href='/download'">Download Log Files</button>
    <button class="danger" onclick="confirmReset()">Reset and Delete SD Card</button>
    <button class="muted" onclick="location.href='/settings'">Settings</button>
    <button class="muted" onclick="location.href='/readme'">Read Me</button>
  </div>
</div>

<script>
function confirmReset() {
  if (!confirm('Wirklich alle Log-Daten auf der SD-Karte löschen?')) return;
  fetch('/reset', { method: 'POST' })
    .then(r => r.text())
    .then(t => alert(t))
    .catch(e => alert('Fehler: ' + e));
}

async function refresh() {
  try {
    const r = await fetch('/api/live');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const d = await r.json();
    document.getElementById('power').textContent =
      d.power === null ? '—' : d.power.toFixed(1);
    document.getElementById('buf').textContent    = d.buffer;
    document.getElementById('drop').textContent   = d.dropped;
    document.getElementById('uptime').textContent = d.uptime;
    const pe = document.getElementById('pzem-status');
    pe.textContent = d.pzem_ok ? 'OK' : 'FEHLER';
    pe.className   = d.pzem_ok ? 'ok' : 'err';
    const se = document.getElementById('sd-status');
    se.textContent = d.sd_ok ? 'OK' : 'FEHLER';
    se.className   = d.sd_ok ? 'ok' : 'err';
  } catch (e) {
    document.getElementById('power').textContent = '—';
  }
}
refresh();
setInterval(refresh, 1000);
</script>

</body></html>
)HTML";

static const char PAGE_SETTINGS[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><title>Settings</title>
<style>body{font-family:sans-serif;max-width:600px;margin:2em auto;padding:1em}
a{color:#007acc}</style></head><body>
<h1>Settings</h1>
<p>Diese Seite ist noch nicht implementiert.</p>
<p><a href="/">← Zurück</a></p>
</body></html>
)HTML";

static const char PAGE_README[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><title>Read Me</title>
<style>body{font-family:sans-serif;max-width:600px;margin:2em auto;padding:1em}
a{color:#007acc}</style></head><body>
<h1>Read Me</h1>
<p>Diese Seite ist noch nicht implementiert.</p>
<p><a href="/">← Zurück</a></p>
</body></html>
)HTML";

// ======================================================

WebPortal::WebPortal(Logger& logger) : _logger(logger), _server(HTTP_PORT) {}

bool WebPortal::begin() {
  Serial.println("[Web] Starte WLAN-AP...");
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD)) {
    Serial.println("[Web] softAP() fehlgeschlagen!");
    return false;
  }
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[Web] AP '%s' aktiv, IP: %s\n",
                WIFI_AP_SSID, ip.toString().c_str());

  _server.on("/",         HTTP_GET,  [this](){ handleRoot(); });
  _server.on("/api/live", HTTP_GET,  [this](){ handleApiLive(); });
  _server.on("/download", HTTP_GET,  [this](){ handleDownload(); });
  _server.on("/reset",    HTTP_POST, [this](){ handleReset(); });
  _server.on("/settings", HTTP_GET,  [this](){ handleSettings(); });
  _server.on("/readme",   HTTP_GET,  [this](){ handleReadme(); });
  _server.onNotFound(                [this](){ handleNotFound(); });

  _server.begin();
  return true;
}

void WebPortal::update() {
  _server.handleClient();
}

// ------------- Handler ----------------

void WebPortal::handleRoot() {
  _server.send_P(200, "text/html", PAGE_INDEX);
}

void WebPortal::handleApiLive() {
  // Statischer Buffer auf dem Stack, keine Heap-Allokation
  char buf[API_BUFFER_SIZE];
  float p = _logger.getLastPower();
  if (isnan(p)) {
    // JSON: null statt 0.0 - so weiß die Webseite, dass kein Messwert da ist
    snprintf(buf, sizeof(buf),
             "{\"power\":null,\"buffer\":%u,\"dropped\":%lu,"
             "\"uptime\":%lu,\"pzem_ok\":%s,\"sd_ok\":%s}",
             (unsigned)_logger.getBufferCount(),
             (unsigned long)_logger.getDroppedSamples(),
             (unsigned long)(millis() / 1000),
             _logger.pzemOk() ? "true" : "false",
             _logger.sdOk()   ? "true" : "false");
  } else {
    snprintf(buf, sizeof(buf),
             "{\"power\":%.1f,\"buffer\":%u,\"dropped\":%lu,"
             "\"uptime\":%lu,\"pzem_ok\":%s,\"sd_ok\":%s}",
             p,
             (unsigned)_logger.getBufferCount(),
             (unsigned long)_logger.getDroppedSamples(),
             (unsigned long)(millis() / 1000),
             _logger.pzemOk() ? "true" : "false",
             _logger.sdOk()   ? "true" : "false");
  }
  _server.send(200, "application/json", buf);
}

void WebPortal::handleDownload() {
  // Vor Download Puffer flushen, damit Datei aktuell ist
  _logger.flushToSD();

  File f = _logger.openLogFileForRead();
  if (!f) {
    _server.send(404, "text/plain", "Log-Datei nicht gefunden oder SD-Fehler.");
    return;
  }
  _server.sendHeader("Content-Type", "text/csv");
  _server.sendHeader("Content-Disposition",
                     "attachment; filename=log.csv");
  // streamFile blockiert ESP32 für die Download-Dauer - bei kleinen
  // Dateien (<1 MB) auf 802.11 unkritisch. Bei größeren Dateien
  // sollte AsyncWebServer verwendet werden.
  _server.streamFile(f, "text/csv");
  f.close();
}

void WebPortal::handleReset() {
  if (_logger.resetSDFile()) {
    _server.send(200, "text/plain",
                 "Log-Datei wurde gelöscht und neu angelegt.");
  } else {
    _server.send(500, "text/plain",
                 "Fehler beim Löschen der Log-Datei (SD nicht verfügbar?).");
  }
}

void WebPortal::handleSettings() {
  _server.send_P(200, "text/html", PAGE_SETTINGS);
}

void WebPortal::handleReadme() {
  _server.send_P(200, "text/html", PAGE_README);
}

void WebPortal::handleNotFound() {
  _server.send(404, "text/plain", "404 Not Found");
}
