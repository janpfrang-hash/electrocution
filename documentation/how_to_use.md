# BRAUN PZEM-004T Power Logger V1 — User Manual

This manual provides instructions for operating, connecting to, and configuring the ESP32-based PZEM-004T Power Logger. Designed specifically for Quality Engineering and appliance product testing, this device tracks electrical telemetry, filters idle standby noise, saves data to an onboard SD card, and serves a localized web dashboard.

---

## 1. Hardware Setup & Status Indicators

### Physical Preparation
1. Ensure a FAT32-formatted micro SD card is inserted into the card slot before powering up the logger.
2. Connect the PZEM-004T module inline with the AC mains lines feeding the kitchen appliance under test.
3. Power on the ESP32 deployment kit.

### LED Visual Diagnostics
The onboard status LED provides immediate feedback on system integrity based on flash frequencies:
* **Normal Operation (1 Hz / 500ms Blink):** System healthy; the PZEM module is responding normally and the SD card filesystem is mounted securely.
* **System Error State (5 Hz / 100ms Fast Blink):** Critical error. Check for a missing or corrupt SD card, or loose serial connection lines between the ESP32 and the PZEM hardware module.

---

## 2. Connecting to the Data Logger

The logger generates an independent, secure Wi-Fi Access Point (AP). You can connect using a smartphone, laptop, or testing tablet directly in the field without needing an external internet connection or local router.

### Step-by-Step Connection Logic
1. Open the **Wi-Fi Settings** menu on your client device.
2. Search the network list and select the SSID: **`PZEM_Logger`**.
3. When prompted, enter the network security password: **`logger1234`**.
4. Wait for your operating system to confirm the Wi-Fi connection.

### Launching the Dashboard Interface
The device utilizes an integrated Captive Portal and catch-all DNS system to intercept client traffic. Upon connecting, your operating system will often automatically present a "Log In to Network" prompt or launch your browser directly to the logging console. 

If the dashboard does not open automatically, launch any standard web browser (Chrome, Safari, Firefox, Edge) and enter one of the following addresses into the URL bar:
* **Direct Gateway IP (Recommended):** `http://192.168.4.1/`
* **Friendly Hostname Endpoint:** `http://braun_PZEM.local`

> **CRITICAL MOBILE DATA NOTE:** Smartphones frequently drop local Wi-Fi connections that lack external internet access, shifting automatically to cellular networks. If the dashboard refuses to load on a smartphone, temporarily turn off **Mobile Data / Cellular Data** in your phone settings while communicating with the data logger.

---

## 3. Main Dashboard Overview

The primary index page offers a lean, high-refresh-rate live panel to monitor appliances during active operational cycles.

* **Live Telemetry:** Displays current active Power Consumption (**W**), True RMS Line Voltage (**V**), and Power Factor (**cos φ**).
* **Hardware Status Fields:**
  * **PZEM:** Displays `OK` or `FEHLER`. If the telemetry line suffers 3 consecutive reading failures, it defaults to an error state. It restores automatically once regular communication blocks resume.
  * **SD:** Displays `OK` or `FEHLER`. If an append cycle fails, the system marks the card as faulty. The background processor automatically runs an isolated re-initialization routine every 30 seconds to recover the file system.
* **Buffers & Counters:**
  * **Puffer (Buffer):** Quantifies rows currently held in the volatile RAM cache (holds a maximum capacity of 64 entries before triggering an automated flush to the SD card).
  * **Verworfen (Dropped):** Keeps a cumulative tally of logged intervals discarded due to missing or damaged SD storage environments.
  * **Uptime:** Monitors relative run-time duration in seconds.

---

## 4. Configuration Panel (Settings Section)

Clicking the **"Settings"** button routes you to the runtime parameter configuration engine. Modifying these limits dynamically optimizes memory constraints and screens raw testing data.

### A. Sampling Rate (Poll Interval)
Configures how frequently the microcontroller reads active values from the serial PZEM module.
* **Available Intervals:** 5/s (200ms), 2/s (500ms), 1/s (1000ms), 0.5/s (2000ms), 0.2/s (5000ms), 1/10s (10000ms), and 1/30s (30000ms).
* **Application Rule:** Higher refresh frequencies (e.g., 5/s) capture precise electrical waveforms during brief transient actions (such as high-surge relay triggers, heating element shifts, or motor startups). Slower frequencies preserve file space over lengthy, multi-hour testing protocols.

### B. Logging Power Threshold
An engineering filter designed to screen out trace background line noise and standby power consumption. The logging system suppresses SD operations entirely if active output power drops below this user-defined value.
* **0 W (No Limit):** Default behavior. Documents every captured metric continuously, regardless of appliance load behavior.
* **1 W / 2 W / 5 W:** Ignores low-level trickle draws from appliance microcontrollers, internal clock displays, or standalone standby circuits.
* **10 W / 20 W / 50 W:** Ensures tracking triggers exclusively when heavy mechanical components (heating elements, water pumps, induction arrays, or compressor motors) draw active load current.

*Note: After making changes, select your desired combination and click **"Apply Settings"**. Runtime definitions take effect instantly and remain active until the logging kit is manually power-cycled or restarted.*

---

## 5. Data Management & Exporting Records

All telemetry points are structured chronologically within a standard file path named `/log.csv` located in the root file directory of the micro SD card.

### File Layout Format
The document records raw information using a comma-delimited structure utilizing the following fixed layout header line:
```csv
time_ms,voltage_V,power_W,cos_phi
