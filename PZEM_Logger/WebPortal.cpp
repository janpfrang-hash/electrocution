/*
 * WebPortal.cpp v5 - heap-effizient + Captive Portal
 * Updated with Power Threshold Setting Controls
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
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Settings – PZEM Logger</title>
<style>
  body { font-family: sans-serif; max-width: 600px; margin: 1em auto;
         padding: 1em; background: #f5f5f5; color: #222; }
  h1   { color: #000; }
  .card { background: white; border-radius: 8px; padding: 1.2em;
          margin-bottom: 1em; box-shadow: 0 1px 3px rgba(0,0,0,.1); }
  h2   { margin: 0 0 1em; font-size: 1.1em; color: #444; }
  .grid-container {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: .5em;
    margin-bottom: 1em;
  }
  .setting-btn {
    padding: .7em .3em;
    font-size: .95em;
    border: 2px solid #ccc;
    border-radius: 6px;
    background: white;
    color: #333;
    cursor: pointer;
    text-align: center;
    transition: border-color .15s, background .15s;
  }
  .setting-btn:hover  { border-color: #007acc; }
  .setting-btn.active { border-color: #007acc; background: #e8f4ff; color: #007acc; font-weight: bold; }
  .current { font-size: .9em; color: #666; margin-bottom: 1.2em; }
  .current span { font-weight: bold; color: #007acc; }
  .save-row { display: flex; gap: .6em; align-items: center; margin-top: 1.5em; }
  button.primary { padding: .8em 1.6em; font-size: 1em; border: none;
                   border-radius: 6px; background: #007acc; color: white; cursor: pointer; }
  button.primary:hover { background: #005f99; }
  button.primary:disabled { background: #aaa; cursor: default; }
  .msg { font-size: .9em; padding: .4em .8em; border-radius: 4px; display: none; }
  .msg.ok  { background: #cfc; color: #060; display: inline-block; }
  .msg.err { background: #fcc; color: #800; display: inline-block; }
  a.back   { display: inline-block; margin-top: .5em; color: #007acc; text-decoration: none; }
  a.back:hover { text-decoration: underline; }
  .note { font-size: .85em; color: #888; margin-top: .5em; }
</style>
</head><body>

<h1>Settings</h1>

<div class="card">
  <h2>Sampling Rate</h2>
  <p class="current">Current rate: <span id="cur-rate">…</span></p>

  <div class="grid-container" id="rate-grid">
    <div class="setting-btn rate-btn" data-ms="200"  >5 / s</div>
    <div class="setting-btn rate-btn" data-ms="500"  >2 / s</div>
    <div class="setting-btn rate-btn" data-ms="1000" >1 / s</div>
    <div class="setting-btn rate-btn" data-ms="2000" >0.5 / s</div>
    <div class="setting-btn rate-btn" data-ms="5000" >0.2 / s</div>
    <div class="setting-btn rate-btn" data-ms="10000">1 / 10 s</div>
    <div class="setting-btn rate-btn" data-ms="30000">1 / 30 s</div>
  </div>

  <hr style="border: 0; border-top: 1px solid #eee; margin: 1.5em 0;">

  <h2>Logging Power Threshold</h2>
  <p class="current">Log data if power exceeds: <span id="cur-thresh">…</span></p>

  <div class="grid-container" id="thresh-grid">
    <div class="setting-btn thresh-btn" data-w="0">0 W (No Limit)</div>
    <div class="setting-btn thresh-btn" data-w="1">1 W</div>
    <div class="setting-btn thresh-btn" data-w="2">2 W</div>
    <div class="setting-btn thresh-btn" data-w="5">5 W</div>
    <div class="setting-btn thresh-btn" data-w="10">10 W</div>
    <div class="setting-btn thresh-btn" data-w="20">20 W</div>
    <div class="setting-btn thresh-btn" data-w="50">50 W</div>
  </div>

  <div class="save-row">
    <button class="primary" id="save-btn" onclick="saveSettings()" disabled>Apply Settings</button>
    <span class="msg" id="msg"></span>
  </div>
  <p class="note">Changes take effect immediately and are kept until the device is restarted.</p>
</div>

<a class="back" href="/">← Back</a>

<script>
let selectedMs = null;
let selectedThresh = null;

// Load current settings from the ESP32
async function loadSettings() {
  try {
    const r = await fetch('/api/settings');
    const d = await r.json();
    selectedMs = d.poll_ms;
    selectedThresh = d.power_threshold;
    updateUI();
  } catch(e) {
    document.getElementById('cur-rate').textContent = 'unknown';
    document.getElementById('cur-thresh').textContent = 'unknown';
  }
}

function msToLabel(ms) {
  const map = {200:'5 / s', 500:'2 / s', 1000:'1 / s',
               2000:'0.5 / s', 5000:'0.2 / s', 10000:'1 / 10 s', 30000:'1 / 30 s'};
  return map[ms] || (ms + ' ms');
}

function updateUI() {
  // Update human readable current values
  document.getElementById('cur-rate').textContent = msToLabel(selectedMs);
  document.getElementById('cur-thresh').textContent = selectedThresh === 0 ? '0 W (No Limit)' : selectedThresh + ' W';

  // Toggle active styling on buttons
  document.querySelectorAll('.rate-btn').forEach(b => {
    b.classList.toggle('active', parseInt(b.dataset.ms) === selectedMs);
  });
  document.querySelectorAll('.thresh-btn').forEach(b => {
    b.classList.toggle('active', parseInt(b.dataset.w) === selectedThresh);
  });
}

// Handle Rate Clicks
document.getElementById('rate-grid').addEventListener('click', e => {
  const btn = e.target.closest('.rate-btn');
  if (!btn) return;
  selectedMs = parseInt(btn.dataset.ms);
  updateUI();
  document.getElementById('save-btn').disabled = false;
  setMsg('', '');
});

// Handle Threshold Clicks
document.getElementById('thresh-grid').addEventListener('click', e => {
  const btn = e.target.closest('.thresh-btn');
  if (!btn) return;
  selectedThresh = parseInt(btn.dataset.w);
  updateUI();
  document.getElementById('save-btn').disabled = false;
  setMsg('', '');
});

async function saveSettings() {
  if (selectedMs === null || selectedThresh === null) return;
  document.getElementById('save-btn').disabled = true;
  try {
    const r = await fetch('/api/settings', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'poll_ms=' + selectedMs + '&power_threshold=' + selectedThresh
    });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const d = await r.json();
    selectedMs = d.poll_ms;
    selectedThresh = d.power_threshold;
    updateUI();
    setMsg('Saved!', 'ok');
  } catch(e) {
    setMsg('Error: ' + e.message, 'err');
    document.getElementById('save-btn').disabled = false;
  }
}

function setMsg(text, cls) {
  const el = document.getElementById('msg');
  el.textContent = text;
  el.className = 'msg ' + cls;
}

loadSettings();
</script>
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

  // ── Captive Portal: DNS catch-all ──
  _dns.start(DNS_PORT, "*", ip);
  Serial.println("[Web] DNS-Server gestartet (catch-all)");

  // ── mDNS: Friendly-Name ──
  if (MDNS.begin(WIFI_AP_HOSTNAME)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("[Web] mDNS aktiv: http://%s.local\n", WIFI_AP_HOSTNAME);
  }

  // ── Captive-Portal-Probes abfangen ──
  _server.on("/generate_204",              HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/gen_204",                   HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/hotspot-detect.html",       HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/library/test/success.html", HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/ncsi.txt",                  HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/connecttest.txt",           HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/redirect",                  HTTP_GET, [this](){ handleCaptivePortal(); });
  _server.on("/canonical.html",            HTTP_GET, [this](){ handleCaptivePortal(); });

  // ── Normale Routen ──
  _server.on("/",              HTTP_GET,  [this](){ handleRoot(); });
  _server.on("/api/live",      HTTP_GET,  [this](){ handleApiLive(); });
  _server.on("/api/settings",  HTTP_GET,  [this](){ handleApiSettings(); });
  _server.on("/api/settings",  HTTP_POST, [this](){ handleApiSettingsSave(); });
  _server.on("/download",      HTTP_GET,  [this](){ handleDownload(); });
  _server.on("/reset",         HTTP_POST, [this](){ handleReset(); });
  _server.on("/settings",      HTTP_GET,  [this](){ handleSettings(); });
  _server.on("/readme",        HTTP_GET,  [this](){ handleReadme(); });
  _server.onNotFound(                     [this](){ handleNotFound(); });

  _server.begin();
  return true;
}

void WebPortal::update() {
  _dns.processNextRequest();
  _server.handleClient();
}

// ------------- Handler ----------------

void WebPortal::handleRoot() {
  _server.send_P(200, "text/html", PAGE_INDEX);
}

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

// GET /api/settings — returns current settings as JSON
void WebPortal::handleApiSettings() {
  char buf[128]; // Increased buffer size slightly to cleanly fit both JSON fields
  snprintf(buf, sizeof(buf),
           "{\"poll_ms\":%lu,\"power_threshold\":%d}",
           (unsigned long)_logger.getPollInterval(),
           (int)_logger.getPowerThreshold());
  _server.send(200, "application/json", buf);
}

// POST /api/settings — body parameters parsed dynamically
void WebPortal::handleApiSettingsSave() {
  // 1. Evaluate Poll Timing Argument
  if (_server.hasArg("poll_ms")) {
    uint32_t ms = (uint32_t)_server.arg("poll_ms").toInt();
    const uint32_t allowed_ms[] = {200, 500, 1000, 2000, 5000, 10000, 30000};
    bool valid = false;
    for (auto v : allowed_ms) { if (ms == v) { valid = true; break; } }
    if (valid) {
      _logger.setPollInterval(ms);
      Serial.printf("[Web] Poll-Intervall gesetzt: %lu ms\n", (unsigned long)ms);
    } else {
      _server.send(400, "application/json", "{\"error\":\"invalid poll_ms\"}");
      return;
    }
  }

  // 2. Evaluate Power Threshold Configuration
  if (_server.hasArg("power_threshold")) {
    int thresh = _server.arg("power_threshold").toInt();
    const int allowed_w[] = {0, 1, 2, 5, 10, 20, 50};
    bool valid = false;
    for (auto v : allowed_w) { if (thresh == v) { valid = true; break; } }
    if (valid) {
      _logger.setPowerThreshold((float)thresh);
      Serial.printf("[Web] Power-Threshold gesetzt: %d W\n", thresh);
    } else {
      _server.send(400, "application/json", "{\"error\":\"invalid power_threshold\"}");
      return;
    }
  }

  // Fall-through back to transmitting the fresh configurations safely
  handleApiSettings();
}

void WebPortal::handleReadme() {
  _server.send_P(200, "text/html", PAGE_README);
}

void WebPortal::handleNotFound() {
  handleCaptivePortal();
}
