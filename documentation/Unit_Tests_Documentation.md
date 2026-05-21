# Electrocution Data Logger: Unit Testing Documentation

This document outlines the testing strategy, coverage, and limitations of the native unit tests implemented for the `Logger` class in the Electrocution Data Logger project.

## Overview

The unit tests are designed to run **natively** on a host machine (e.g., Linux via GitHub Actions) rather than on the ESP32 hardware. This allows for fast, continuous integration testing without requiring physical devices. 

The tests utilize the **doctest** C++ testing framework and rely on a custom Hardware Abstraction Layer (HAL) mock (`arduino_mock.h`) to simulate the Arduino environment (e.g., `millis()`, `Serial`, `SPI`, `SD`, and `PZEM004Tv30`).

---

## What the Unit Tests Cover (The Logic)

The native unit tests strictly evaluate the internal business logic and state machines of the `Logger` class.

### 1. Configuration Sanity
- Validates that time intervals, buffer sizes, and error thresholds defined in `Config.h` remain within safe and expected operational limits (e.g., avoiding memory overflows).
- Ensures the CSV header string strictly matches the required data columns (`millis`, `voltage_V`, `power_W`, `pf`).

### 2. RAM Ring-Buffer Management
- **Initialization:** Verifies the buffer starts empty with zero dropped samples.
- **Filling:** Ensures samples are correctly appended to the buffer up to `RAM_BUFFER_SIZE`.
- **Overflow Handling:** Confirms that when the SD card is unavailable and the buffer overflows, the oldest samples are dropped, the newest samples are retained at the end of the buffer, and the drop counter increments accurately.

### 3. PZEM Error State Machine
- Tests the error tracking logic to ensure the `pzemOk()` flag only turns `false` after exceeding the `PZEM_ERROR_THRESHOLD` of consecutive failed readings.
- Validates that a single successful reading clears the error counter and restores the `pzemOk()` state to `true`.
- Checks edge cases (e.g., exactly at the threshold vs. one below).

### 4. Data Storage and Retrieval
- Ensures `Sample` structs correctly store and retrieve timestamp, voltage, power, and power factor data.
- Verifies that getter methods (`getLastVoltage()`, `getLastPower()`, `getLastPf()`) return `NaN` before the first valid reading, and return exact stored values afterward.

### 5. SD Card State & System Health
- Ensures `flushToSD()` safely aborts and returns `false` if the SD card is flagged as unavailable.
- Validates the overarching `ok()` method, which must strictly return `false` if *either* the PZEM module is repeatedly failing *or* the SD card is disconnected.

---

## What is NOT Covered (The Hardware)

Because these tests run natively without real hardware, they **do not** verify physical interactions or hardware-dependent timings. The following aspects are intentionally excluded from native unit testing and require physical ESP32 testing:

### 1. Actual Hardware Communication
- **SPI Protocol:** The actual transmission of data over the SPI bus to the SD card reader is not tested.
- **UART Protocol:** Real serial communication with the PZEM-004T module (baud rates, timing, checksums) is bypassed using mock injected values.

### 2. Physical File I/O
- The tests do not interact with a real FAT32/exFAT filesystem. 
- Real-world SD card issues like write latency, corrupted filesystems, or bad sectors are not simulated. The `File` and `SDClass` objects are merely empty stubs.

### 3. Real-Time Execution & Timing
- The `millis()` function is mocked. Therefore, the exact real-world timing of `pollIfDue()` and `flushIfDue()` loops is not strictly validated here.

### 4. ESP32 Memory Constraints (Heap/Stack)
- While the test checks `RAM_BUFFER_SIZE` limits logically, it runs in a standard Linux process with gigabytes of RAM. Real-world ESP32 heap fragmentation or stack overflows are not caught by these native tests.

### 5. Web Portal & WiFi
- Network connectivity, captive portal routing, and HTTP API responses (`WebPortal.cpp`) are currently outside the scope of `test_logger.cpp`.
