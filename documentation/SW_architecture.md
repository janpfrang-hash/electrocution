# PZEM Logger v4 — Architectural Design Document

**ESP32-WROOM · Arduino Framework · Firmware v4**

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Architectural Overview](#2-architectural-overview)
3. [Module Details](#3-module-details)
4. [Execution Model](#4-execution-model)
5. [Data Formats](#5-data-formats)
6. [Inter-Module Dependencies](#6-inter-module-dependencies)
7. [Key Design Decisions](#7-key-design-decisions)
8. [Known Limitations & Future Work](#8-known-limitations--future-work)

---

## 1. System Overview

The PZEM Logger is a self-contained embedded data-acquisition and monitoring system running on an ESP32-WROOM microcontroller. It continuously samples voltage, active power, and power factor from a Peacefair PZEM-004T v3 energy meter, stores measurements on a micro-SD card, and exposes a live web dashboard over a built-in Wi-Fi Access Point.

### 1.1 Purpose and Scope

- Measure and log AC electrical parameters: voltage (V), active power (W), power factor (cos φ).
- Store data persistently on SD card in CSV format with millisecond timestamps.
- Provide a real-time web interface accessible from any device connected to the device's Wi-Fi AP.
- Operate reliably without any network infrastructure — completely standalone (AP mode, captive portal).

### 1.2 Target Hardware

| Component | Part | Interface |
|---|---|---|
| Microcontroller | ESP32-WROOM-32 | — |
| Energy Meter | PZEM-004T v3.0 | UART (Serial2) RX=16 TX=17 |
| Storage | Micro-SD card | SPI (VSPI) CS=25 CLK=27 MOSI=14 MISO=26 |
| Status indicator | LED | GPIO 32 |
| User input | Push button (future) | GPIO 33 (INPUT_PULLUP) |
| Supply sense (future) | Analog divider | GPIO 35 (ADC, 12-bit) |

---

## 2. Architectural Overview

The firmware is structured into four loosely coupled modules plus a central configuration header. Each module owns a distinct concern; the main sketch wires them together in a bare-metal cooperative scheduling loop.

### 2.1 Module Map

| File | Module | Responsibility | Key dependency |
|---|---|---|---|
| `Config.h` | Configuration | All pins, intervals, limits, string constants | — (no includes) |
| `StatusLed.h` | StatusLed | Blink pattern driven by system health | Config.h |
| `Logger.h` | Logger | PZEM polling, RAM buffer, SD persistence | PZEM004Tv30, SD, SPI |
| `WebPortal.h/.cpp` | WebPortal | HTTP server, DNS catch-all, mDNS, live API | Logger (read-only ref) |
| `PZEM_Logger.ino` | Main sketch | `setup()` / `loop()` — orchestration only | All modules |

### 2.2 Abstraction Layers

The design follows a three-layer model:

| Layer | Module(s) | Description |
|---|---|---|
| **L1 — Hardware** | PZEM004Tv30 lib, SD lib, SPI, WiFi | Vendor/platform libraries. Logger and WebPortal call these directly; the rest of the firmware never touches hardware registers. |
| **L2 — Service** | Logger, WebPortal, StatusLed | Single-responsibility classes. Each encapsulates its own state and exposes a minimal public API. No cross-service calls except WebPortal → Logger (read-only). |
| **L3 — Application** | PZEM_Logger.ino | Thin orchestration layer. Calls `pollIfDue()`, `flushIfDue()`, `update()` and `setOk()` in a tight cooperative loop. Contains no business logic. |

---

## 3. Module Details

### 3.1 `Config.h` — Central Configuration

A pure preprocessor header. Defines every tunable constant in one place so that no magic numbers appear anywhere else in the codebase.

| Constant | Value | Purpose |
|---|---|---|
| `PIN_PZEM_RX` / `TX` | 16 / 17 | UART2 pins for PZEM meter |
| `PIN_SD_CS/CLK/MOSI/MISO` | 25/27/14/26 | VSPI bus for SD card |
| `INTERVAL_PZEM_POLL_MS` | 500 | How often the PZEM is read (2 Hz) |
| `INTERVAL_SD_FLUSH_MS` | 10 000 | How often RAM buffer is written to SD |
| `PZEM_ERROR_THRESHOLD` | 3 | Consecutive NaN reads before PZEM is declared faulty |
| `RAM_BUFFER_SIZE` | 64 | Max samples in RAM (64 × 8 B = 512 B, covers ~32 s) |
| `WIFI_AP_SSID` | `PZEM_Logger` | Access point SSID visible to clients |
| `WIFI_AP_HOSTNAME` | `braun_PZEM` | mDNS hostname → `http://braun_PZEM.local` |
| `API_BUFFER_SIZE` | 256 | Stack-allocated JSON response buffer (prevents heap fragmentation) |

---

### 3.2 `StatusLed.h` — Health Indicator

Drives a single LED with two distinct blink patterns that communicate overall system health without requiring a display or serial monitor.

| State | Pattern | Meaning |
|---|---|---|
| `LED_OK` | 1 Hz blink (500 ms) | PZEM reachable AND SD card healthy |
| `LED_ERROR` | 5 Hz blink (100 ms) | PZEM unreachable OR SD card failed |

**Design notes:**

- `setState()` is idempotent — calling it every loop with the same value causes no phase reset. The blink timer resets only on an actual state transition.
- `setOk(bool)` is a convenience wrapper over `setState()` used by the main loop.
- `update()` uses non-blocking `millis()` comparison; no `delay()` calls anywhere in the system.

---

### 3.3 `Logger.h` — Acquisition, Buffering & Persistence

The most complex module. It owns the PZEM sensor driver, the RAM ring buffer, and the SD card writer. Designed to tolerate failures in either the sensor or the storage without crashing.

#### 3.3.1 Data Flow

The acquisition pipeline has three stages:

| # | Stage | Description |
|---|---|---|
| 1 | PZEM poll | Every 500 ms: read voltage, power, pf via UART. NaN = sensor absent or busy. |
| 2 | RAM buffer | Valid samples are pushed into a 64-element static array (`Sample` structs, ~8 B each). |
| 3 | SD flush | Every 10 s: all buffered samples are appended to `/log.csv` as CSV rows. |

#### 3.3.2 Fault Tolerance

**PZEM:**
- Isolated read failures (NaN) increment a saturation counter (`_pzemErrorCount`, max 255).
- The sensor is only declared faulty once the counter reaches `PZEM_ERROR_THRESHOLD` (3). Single glitches are silently absorbed.
- Recovery is automatic: the counter resets to 0 on the next valid read.

**SD card:**
- If `flushToSD()` fails to open the file, `_sdOk` is set to `false`.
- With `_sdOk = false`, the RAM buffer acts as the sole storage. On overflow, the oldest sample is evicted (FIFO drop) and `_droppedSamples` is incremented.
- `tryRecoverSD()` is called every 30 s — it runs `SD.end()` + `SD.begin()` and recreates the CSV header if needed.
- On successful recovery, normal flush resumes immediately.

#### 3.3.3 Buffer Overflow Strategy

```
pushSample() logic:
  if (buffer not full)          → append
  else if (SD flush succeeds)   → flush, then append
  else                          → drop oldest (memmove), append newest, ++droppedSamples
```

#### 3.3.4 Public API

| Method | Description |
|---|---|
| `begin()` | Init SPI bus, mount SD, reset error counters. |
| `pollIfDue()` | Non-blocking poll: reads PZEM if 500 ms have elapsed. |
| `flushIfDue()` | Non-blocking flush: writes RAM buffer to SD every 10 s; triggers SD recovery if needed. |
| `flushToSD()` | Immediate forced flush (called by WebPortal before download). |
| `resetSDFile()` | Delete and recreate `/log.csv` with header (triggered by web reset button). |
| `openLogFileForRead()` | Returns SD `File` handle for HTTP streaming. |
| `pzemOk()` / `sdOk()` / `ok()` | Health status queries used by StatusLed and WebPortal. |
| `getLastPower/Voltage/Pf()` | Most recent valid measurements (`NAN` if sensor not yet seen). |
| `getBufferCount()` / `getDroppedSamples()` | Buffer diagnostics exposed in the live API. |

---

### 3.4 `WebPortal.h/.cpp` — HTTP Server & Captive Portal

Runs an HTTP/1.1 server on port 80, a DNS catch-all server (port 53), and an mDNS responder. Holds a `const` reference to `Logger` and never writes to it (single-directional data coupling).

#### 3.4.1 Network Stack

| Service | Behaviour |
|---|---|
| Wi-Fi AP | SSID: `PZEM_Logger`, WPA2 password, fixed IP `192.168.4.1`. |
| DNS (port 53) | Wildcard `*` → `192.168.4.1`. Forces all DNS lookups to the device (captive portal mechanics). |
| mDNS | Hostname `braun_PZEM` → reachable as `http://braun_PZEM.local` on networks that support mDNS. |
| HTTP (port 80) | WebServer library, single-threaded, polled in `loop()`. |

#### 3.4.2 HTTP Routes

| Path | Method | Description |
|---|---|---|
| `/` | GET | Serves main dashboard HTML from PROGMEM (no heap copy via `send_P`). |
| `/api/live` | GET | Returns JSON with `power`, `voltage`, `pf`, `buffer`, `dropped`, `uptime`, `pzem_ok`, `sd_ok`. Uses a 256-byte stack buffer. |
| `/download` | GET | Forces a RAM flush, then streams `/log.csv` as attachment via `streamFile()`. |
| `/reset` | POST | Calls `logger.resetSDFile()`; responds with plain-text status. |
| `/settings` | GET | Placeholder page (not yet implemented). |
| `/readme` | GET | Static info page listing features and contact. |
| Captive probes | GET | 8 OS-specific probe URLs (Android `/gen_204`, Apple `/hotspot-detect.html`, Windows `/ncsi.txt`, Firefox `/canonical.html`, etc.) all redirect `302` → `/`. |
| `*` (not found) | ANY | Unknown paths also redirect to `/` to prevent browser error pages. |

#### 3.4.3 Frontend Architecture

The web UI is a single-page application embedded in PROGMEM as a raw string literal (`PAGE_INDEX`). No external CDN, no build step.

- Data is not server-side rendered. The HTML page is static; all live values arrive via `fetch('/api/live')` called every 1 000 ms by a JavaScript `setInterval`.
- The JSON API returns `null` for measurements when the sensor reports NaN, and the JS renders `—` in those cases.
- The reset action uses a `fetch` POST with a confirmation dialog; no `<form>` tags are involved.

#### 3.4.4 Memory Strategy

- HTML pages stored in PROGMEM (flash), sent with `server.send_P()` — zero heap allocation.
- JSON response uses a 256-byte `char` array on the stack (`API_BUFFER_SIZE`) — no `String` objects, no heap fragmentation.
- File streaming uses `WebServer::streamFile()`, which reads from SD in small chunks — no full file buffered in RAM.

---

## 4. Execution Model

### 4.1 Cooperative Scheduling

There is no RTOS. `loop()` runs continuously and calls each module's non-blocking update method. Each method checks whether its scheduled interval has elapsed using `millis()` and returns immediately if not.

| Call | Period | Worst-case blocking time |
|---|---|---|
| `logger.pollIfDue()` | 500 ms | ~10 ms (UART read, 9600 baud, PZEM frame ~19 bytes) |
| `logger.flushIfDue()` | 10 000 ms | ~50–200 ms (SD write, 64 rows × ~25 bytes at 4 MHz SPI) |
| `webPortal.update()` | every loop | < 1 ms typical; spikes on `/download` (SD streaming) |
| `statusLed.setOk()` | every loop | < 1 µs (idempotent state check) |
| `statusLed.update()` | every loop | < 1 µs (`millis` compare + `digitalWrite`) |

### 4.2 Startup Sequence

1. Serial UART0 opened at 115 200 baud (debug output).
2. Pin modes configured (BUTTON pull-up, VSUPPLY input, 12-bit ADC).
3. `StatusLed` initialised, set to ERROR blink (fast) to signal ongoing init.
4. `Logger.begin()`: SPI bus initialised, `SD.begin()` attempted at 4 MHz.
5. If SD OK: `ensureLogHeader()` creates `/log.csv` with CSV header if file absent.
6. `WebPortal.begin()`: Wi-Fi AP started, DNS server, mDNS, HTTP routes registered.
7. `loop()` entered — LED transitions to OK blink once `Logger.ok()` returns `true`.

---

## 5. Data Formats

### 5.1 CSV Log File (`/log.csv`)

Written to the SD card root. Each row is appended by `flushToSD()`. The file is never rewritten in-place — only appended or fully recreated on reset.

```csv
millis,voltage_V,power_W,pf
1234500,229.8,342.5,0.97
1235000,230.1,341.0,0.97
```

| Column | Unit / type | Notes |
|---|---|---|
| `millis` | ms (uint32) | ESP32 uptime counter. Wraps at ~49.7 days. |
| `voltage_V` | V (float, 1 dp) | RMS AC voltage from PZEM. |
| `power_W` | W (float, 1 dp) | Active power from PZEM. |
| `pf` | — (float, 2 dp) | Power factor cos φ, range 0.00–1.00. |

### 5.2 Live API JSON (`GET /api/live`)

```json
{
  "power": 342.5,
  "voltage": 229.8,
  "pf": 0.97,
  "buffer": 12,
  "dropped": 0,
  "uptime": 3720,
  "pzem_ok": true,
  "sd_ok": true
}
```

`power`, `voltage`, and `pf` are `null` when the PZEM has not yet delivered a valid reading (NaN guard in firmware).

---

## 6. Inter-Module Dependencies

The dependency graph is strictly acyclic:

```
                   Config.h
                  ↗    ↑    ↖
           StatusLed  Logger  WebPortal
                         ↑
                    WebPortal (read-only reference)

PZEM_Logger.ino  →  StatusLed, Logger, WebPortal
```

| From | To | Nature |
|---|---|---|
| main sketch | Logger | Calls `pollIfDue()`, `flushIfDue()`, `sdOk()`, `ok()` |
| main sketch | WebPortal | Calls `begin()`, `update()` |
| main sketch | StatusLed | Calls `begin()`, `setOk(logger.ok())`, `update()` |
| WebPortal | Logger | `const` ref — reads `getLastPower/Voltage/Pf()`, `getBufferCount()`, `getDroppedSamples()`, `pzemOk()`, `sdOk()`, `flushToSD()`, `resetSDFile()`, `openLogFileForRead()` |
| Logger | Config.h | Pin definitions, intervals, buffer size, file path, error threshold |
| WebPortal | Config.h | SSID, password, hostname, ports, API buffer size |
| StatusLed | Config.h | `PIN_LED`, LED blink intervals |

---

## 7. Key Design Decisions

| Decision | Rationale |
|---|---|
| **No RTOS / no tasks** | All operations complete in < 200 ms worst case. A cooperative loop avoids stack overhead, priority inversion, and semaphore complexity. Suitable for a single-core polling workload. |
| **RAM ring buffer before SD** | SD open/close on every sample would wear the FAT, cause high latency, and fail silently on busy sectors. Batching 64 samples (~32 s) reduces SD operations by 64×. |
| **PROGMEM HTML, stack JSON** | ESP32 has ~320 KB RAM. Large `String` objects from heap fragmentation cause random crashes. PROGMEM keeps HTML in flash; stack buffers avoid `malloc`/`free` for API responses. |
| **AP mode (no STA)** | Device is deployed on machines without known Wi-Fi infrastructure. AP mode requires zero configuration and works in any environment. |
| **Captive portal + mDNS** | Most OS auto-open the browser on captive portal detection. mDNS provides a friendly URL (`braun_PZEM.local`) for users who dismiss the captive portal. |
| **PZEM error threshold = 3** | The PZEM-004T can return NaN on a single read due to line noise or measurement settling. A threshold of 3 absorbs transient glitches without masking real hardware failures. |
| **SD auto-recovery every 30 s** | A hot-swap or power glitch on the SD card sets `_sdOk = false`. Periodic re-init means the device self-heals when the card is reinserted without a reboot. |
| **FIFO drop on buffer overflow** | When both RAM and SD are exhausted, the oldest sample is dropped (not the newest). Retaining the most recent data is more useful for fault diagnosis. |

---

## 8. Known Limitations & Future Work

| Item | Notes |
|---|---|
| `millis()` overflow | `uint32` wraps after ~49.7 days. CSV timestamps will reset; interval timers handle wrap correctly via subtraction arithmetic, but absolute timestamps will appear discontinuous. |
| Single log file | All data goes to `/log.csv`. Long deployments produce a large single file. Log rotation (by date or size) is not yet implemented. |
| No RTC | Timestamps are milliseconds since boot, not wall-clock time. An I2C RTC module (e.g. DS3231) or NTP over STA mode would provide absolute timestamps. |
| Settings page stub | `/settings` returns a placeholder. Poll interval, flush interval, and power threshold are currently compile-time constants in `Config.h` only. |
| `PIN_BUTTON` unused | GPIO 33 is configured `INPUT_PULLUP` but never read. Intended for future local actions (e.g. manual flush or LED test). |
| `PIN_VSUPPLY` unused | GPIO 35 ADC is initialised but never sampled. Intended for supply voltage monitoring. |
| Single HTTP client | `WebServer` library is single-threaded. A second browser tab polling `/api/live` every second while a `/download` is in progress may stall. |
