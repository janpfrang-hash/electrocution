/*
 * WebPortal.cpp v5 - heap-effizient + Captive Portal
 *
 * Änderungen gegenüber v4:
 *  - DNSServer catch-all: alle DNS-Anfragen → 192.168.4.1
 *  - mDNS: erreichbar unter http://braun_PZEM.local
 *  - Captive-Portal-Probes aller gängigen OS abgefangen (Android, Apple, Windows, Firefox)
 *  - handleCaptivePortal() leitet per 302 auf Root weiter
 *  - update() ruft _dns.processNextRequest() auf
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
  h1 { color: #000; }
  .card { background: white; border-radius: 8px; padding: 1em;
          margin-bottom: 1em; box-shadow: 0 1px 3px rgba(0,0,0,.1); }
  .live { font-size: 2.5em; font-weight: bold; color: #007acc;
          text-align: center; margin: .3em 0; }
  .sub-live { font-size: 1.2em; font-weight: normal; color: #444;
              text-align: center; margin-bottom: 0.5em; }
  .label { text-align: center; color: #666; font-weight: bold; }
  .btn-row { display: grid; grid-template-columns: 1fr 1fr; gap: .6em; }
  button { padding: .9em; font-size: 1em; border: none; border-radius: 6px;
           background: #007acc; color: white; cursor: pointer; }
  button:hover { background: #005f99; }
  button.danger { background: #d33; }
  button.danger:hover { background: #a22; }
  button.muted { background: #888; }
  .status { font-size: .9em; color: #666; margin-top: .8em; text-align: center; }
  .status span { display: inline-block; padding: .1em .5em;
                 border-radius: 3px; margin-right: .3em; }
  .ok  { background: #cfc; color: #060; }
  .err { background: #fcc; color: #800; }
</style>
</head><body>

<h1>BRAUN PZEM-004 Logger</h1>

<div class="card">
  <div class="label">Aktuelle Werte</div>
  <div class="live"><span id="power">—</span> W</div>
  <div class="sub-live"><span id="voltage">—</span> V &nbsp;|&nbsp; cos φ: <span id="pf">—</span></div>
  <hr style="border: 0; border-top: 1px solid #eee; margin: 1em 0;">
  <div class="status">
    PZEM: <span id="pzem-status" class="ok">?</span>
    SD:   <span id="sd-status"   class="ok">?</span><br><br>
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
    document.getElementById('power').textContent = d.power === null ? '—' : d.power.toFixed(1);
    document.getElementById('voltage').textContent = d.voltage === null ? '—' : d.voltage.toFixed(1);
    document.getElementById('pf').textContent = d.pf === null ? '—' : d.pf.toFixed(2);
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
    document.getElementById('voltage').textContent = '—';
    document.getElementById('pf').textContent = '—';
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
<style>body{font-family:sans-serif;max-width:600px;margin:2em auto;padding:1em;line-height:1.5;}
a{color:#007acc}</style></head><body>
<h1>BBRAUN PZEM-004T Power Logger V1</h1>
<p><strong>Functions:</strong></p>
<ul>
  <li>logs time, voltage, power, cos phi</li>
  <li>adjustable poll, and log frequency</li>
  <li>adjustable power threshold for start of log</li>
  <li>polls per default every 0.5 s</li>
  <li>logs values per default every 10 s to SD memory card</li>
  <li>transmitts values live via wifi</li>
</ul>
<br>
<p><strong>Contact:</strong> jan.pfrang@delonghigroup.com</p>
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

  // ── Captive Portal: DNS catch-all → alle Domains zeigen auf uns ──
  _dns.start(DNS_PORT, "*", ip);
  Serial.println("[Web] DNS-Server gestartet (catch-all)");

  // ── mDNS: http://braun_PZEM.local als Friendly-Name ──
  if (MDNS.begin(WIFI_AP_HOSTNAME)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("[Web] mDNS aktiv: http://%s.local\n", WIFI_AP_HOSTNAME);
  }

  // ── Captive-Portal-Probes der Betriebssysteme abfangen ──
  // Android
  _server.on("/generate_204",              HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/gen_204",                   HTTP_GET, [this](){ handleCaptivePortal(); });
  // Apple (iOS / macOS)
  _server.on("/hotspot-detect.html",       HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/library/test/success.html", HTTP_GET, [this](){ handleCaptivePortal(); });
  // Windows
  _server.on("/ncsi.txt",                  HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/connecttest.txt",           HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/redirect",                  HTTP_GET, [this](){ handleCaptivePortal(); });
  // Firefox
  _server.on("/canonical.html",            HTTP_GET, [this](){ handleCaptivePortal(); });

  // ── Normale Routen (unverändert) ──
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
  _dns.processNextRequest();   // DNS-Anfragen beantworten (Captive Portal)
  _server.handleClient();
}

// ------------- Handler ----------------

void WebPortal::handleRoot() {
  _server.send_P(200, "text/html", PAGE_INDEX);
}

// Captive-Portal-Redirect: OS erkennt fehlende Internetverbindung und öffnet Browser
void WebPortal::handleCaptivePortal() {
  _server.sendHeader("Location", "http://192.168.4.1/", true);
  _server.send(302, "text/plain", "");
}

void WebPortal::handleApiLive() {
  char buf[API_BUFFER_SIZE];
  float p  = _logger.getLastPower();
  float v  = _logger.getLastVoltage();
  float pf = _logger.getLastPf();

  if (isnan(p) || isnan(v) || isnan(pf)) {
    snprintf(buf, sizeof(buf),
             "{\"power\":null,\"voltage\":null,\"pf\":null,\"buffer\":%u,\"dropped\":%lu,"
             "\"uptime\":%lu,\"pzem_ok\":%s,\"sd_ok\":%s}",
             (unsigned)_logger.getBufferCount(),
             (unsigned long)_logger.getDroppedSamples(),
             (unsigned long)(millis() / 1000),
             _logger.pzemOk() ? "true" : "false",
             _logger.sdOk()   ? "true" : "false");
  } else {
    snprintf(buf, sizeof(buf),
             "{\"power\":%.1f,\"voltage\":%.1f,\"pf\":%.2f,\"buffer\":%u,\"dropped\":%lu,"
             "\"uptime\":%lu,\"pzem_ok\":%s,\"sd_ok\":%s}",
             p, v, pf,
             (unsigned)_logger.getBufferCount(),
             (unsigned long)_logger.getDroppedSamples(),
             (unsigned long)(millis() / 1000),
             _logger.pzemOk() ? "true" : "false",
             _logger.sdOk()   ? "true" : "false");
  }
  _server.send(200, "application/json", buf);
}

void WebPortal::handleDownload() {
  _logger.flushToSD();

  File f = _logger.openLogFileForRead();
  if (!f) {
    _server.send(404, "text/plain", "Log-Datei nicht gefunden oder SD-Fehler.");
    return;
  }
  _server.sendHeader("Content-Type", "text/csv");
  _server.sendHeader("Content-Disposition",
                     "attachment; filename=log.csv");
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
  // Unbekannte Pfade → Captive Portal (verhindert Browser-Fehlermeldungen)
  handleCaptivePortal();
}