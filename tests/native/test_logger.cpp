/*
 * test_logger.cpp
 *
 * Native unit tests for Logger.h — runs on Linux, no hardware needed.
 *
 * Test framework: single-header doctest (downloaded by CMake).
 *
 * What is tested here (pure logic, no hardware):
 *   - Ring-buffer fill and drop counting
 *   - PZEM error threshold / recovery
 *   - setPollInterval / getPollInterval
 *   - pzemOk() state machine
 *   - getLastVoltage / Power / Pf update
 *   - getBufferCount / getDroppedSamples
 *   - flushToSD() returns false when SD not OK
 *   - CSV header constant content
 *   - Config.h values are within expected ranges
 *
 * NOT tested here (require real hardware or extensive mocking):
 *   - Actual SD write (File I/O)
 *   - Actual PZEM UART communication
 *   - pollIfDue() timing (depends on millis())
 */

// ── Tell doctest to generate its main() in this TU ──────────────────────────
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

// ── Redirect Arduino HAL to our stubs ───────────────────────────────────────
#include "arduino_mock.h"

// ── Code under test ──────────────────────────────────────────────────────────
// We need to expose private members for white-box testing.
// The cleanest approach for a header-only class: use a friend test accessor.
#define private public   // accepted practice for unit tests of C++ embedded code
#include "../../PZEM_Logger/Config.h"
#include "../../PZEM_Logger/Logger.h"
#undef private

// ═══════════════════════════════════════════════════════════════════════════
// Helper: build a Logger with known PZEM readings injected
// ═══════════════════════════════════════════════════════════════════════════
struct TestLogger {
    Logger logger;

    // Directly push a sample (bypasses PZEM / SD)
    void pushSampleDirect(float v, float p, float pf) {
        logger._lastVoltage = v;
        logger._lastPower   = p;
        logger._lastPf      = pf;
        Sample s { 0, v, p, pf };
        logger.pushSample(s);
    }

    PZEM004Tv30& pzem() { return logger._pzem; }
};

// ═══════════════════════════════════════════════════════════════════════════
// Config sanity
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("Config.h: constants are within expected ranges") {
    CHECK(INTERVAL_PZEM_POLL_MS     >= 200);
    CHECK(INTERVAL_PZEM_POLL_MS     <= 30000);
    CHECK(INTERVAL_SD_FLUSH_MS      >= 1000);
    CHECK(RAM_BUFFER_SIZE            >= 8);
    CHECK(RAM_BUFFER_SIZE            <= 512);
    CHECK(PZEM_ERROR_THRESHOLD       >= 1);
    CHECK(PZEM_ERROR_THRESHOLD       <= 10);
    CHECK(HTTP_PORT                  == 80);
    CHECK(API_BUFFER_SIZE            >= 64);
}

TEST_CASE("Config.h: CSV header contains required column names") {
    const char* hdr = LOG_FILE_HEADER;
    CHECK(std::string(hdr).find("time_ms")   != std::string::npos);
    CHECK(std::string(hdr).find("voltage_V") != std::string::npos);
    CHECK(std::string(hdr).find("power_W")   != std::string::npos);
    CHECK(std::string(hdr).find("cos_phi")   != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════
// Poll interval (runtime-adjustable)
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("setPollInterval: default equals INTERVAL_PZEM_POLL_MS") {
    Logger l;
    CHECK(l.getPollInterval() == INTERVAL_PZEM_POLL_MS);
}

TEST_CASE("setPollInterval: updates correctly") {
    Logger l;
    l.setPollInterval(1000);
    CHECK(l.getPollInterval() == 1000);
    l.setPollInterval(5000);
    CHECK(l.getPollInterval() == 5000);
}

TEST_CASE("setPollInterval: ignores zero (guard against division/underflow)") {
    Logger l;
    l.setPollInterval(2000);
    l.setPollInterval(0);           // should be rejected
    CHECK(l.getPollInterval() == 2000);
}

TEST_CASE("setPollInterval: accepts all seven UI values") {
    Logger l;
    uint32_t valid[] = {200, 500, 1000, 2000, 5000, 10000, 30000};
    for (auto ms : valid) {
        l.setPollInterval(ms);
        CHECK(l.getPollInterval() == ms);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Ring buffer — fill and overflow
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("Buffer: starts empty") {
    Logger l;
    CHECK(l.getBufferCount()    == 0);
    CHECK(l.getDroppedSamples() == 0);
}

TEST_CASE("Buffer: fills to RAM_BUFFER_SIZE without dropping (SD unavailable)") {
    TestLogger tl;
    tl.logger._sdOk = false;

    for (size_t i = 0; i < RAM_BUFFER_SIZE; i++) {
        tl.pushSampleDirect(230.0f, (float)i, 0.95f);
    }
    CHECK(tl.logger.getBufferCount()    == RAM_BUFFER_SIZE);
    CHECK(tl.logger.getDroppedSamples() == 0);
}

TEST_CASE("Buffer: oldest sample dropped when full and SD unavailable") {
    TestLogger tl;
    tl.logger._sdOk = false;

    // Fill the buffer
    for (size_t i = 0; i < RAM_BUFFER_SIZE; i++) {
        tl.pushSampleDirect(230.0f, (float)i, 0.95f);
    }
    // One more — should trigger a drop
    tl.pushSampleDirect(230.0f, 999.0f, 0.95f);

    CHECK(tl.logger.getBufferCount()    == RAM_BUFFER_SIZE); // size unchanged
    CHECK(tl.logger.getDroppedSamples() == 1);

    // Newest sample should be at the end of the buffer
    CHECK(tl.logger._buffer[RAM_BUFFER_SIZE - 1].power_W == doctest::Approx(999.0f));
}

TEST_CASE("Buffer: drop counter increments on each overflow") {
    TestLogger tl;
    tl.logger._sdOk = false;

    for (size_t i = 0; i < RAM_BUFFER_SIZE + 5; i++) {
        tl.pushSampleDirect(230.0f, (float)i, 0.95f);
    }
    CHECK(tl.logger.getDroppedSamples() == 5);
}

// ═══════════════════════════════════════════════════════════════════════════
// PZEM error threshold state machine
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("pzemOk: starts true") {
    Logger l;
    CHECK(l.pzemOk() == true);
}

TEST_CASE("pzemOk: false only after PZEM_ERROR_THRESHOLD consecutive errors") {
    Logger l;
    // Simulate errors by incrementing the counter directly
    for (int i = 0; i < PZEM_ERROR_THRESHOLD - 1; i++) {
        l._pzemErrorCount++;
        CHECK(l.pzemOk() == true);   // still OK below threshold
    }
    l._pzemErrorCount++;              // now at threshold
    CHECK(l.pzemOk() == false);
}

TEST_CASE("pzemOk: recovers after counter reset") {
    Logger l;
    l._pzemErrorCount = PZEM_ERROR_THRESHOLD;
    CHECK(l.pzemOk() == false);
    l._pzemErrorCount = 0;
    CHECK(l.pzemOk() == true);
}

TEST_CASE("pzemOk: threshold boundary — exactly at limit is false") {
    Logger l;
    l._pzemErrorCount = PZEM_ERROR_THRESHOLD;
    CHECK(l.pzemOk() == false);
}

TEST_CASE("pzemOk: one below threshold is still true") {
    Logger l;
    l._pzemErrorCount = PZEM_ERROR_THRESHOLD - 1;
    CHECK(l.pzemOk() == true);
}

// ═══════════════════════════════════════════════════════════════════════════
// Last-reading getters
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("Getters: NaN before first successful reading") {
    Logger l;
    CHECK(std::isnan(l.getLastVoltage()));
    CHECK(std::isnan(l.getLastPower()));
    CHECK(std::isnan(l.getLastPf()));
}

TEST_CASE("Getters: return correct values after reading is stored") {
    Logger l;
    l._lastVoltage = 231.5f;
    l._lastPower   = 850.0f;
    l._lastPf      = 0.97f;

    CHECK(l.getLastVoltage() == doctest::Approx(231.5f));
    CHECK(l.getLastPower()   == doctest::Approx(850.0f));
    CHECK(l.getLastPf()      == doctest::Approx(0.97f));
}

// ═══════════════════════════════════════════════════════════════════════════
// SD state
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("sdOk: false before begin()") {
    Logger l;
    CHECK(l.sdOk() == false);
}

TEST_CASE("flushToSD: returns false (not ok) when SD not available") {
    Logger l;
    // SD not OK, nothing to flush
    CHECK(l.flushToSD() == false);
}

TEST_CASE("ok(): false when PZEM error threshold reached") {
    Logger l;
    l._sdOk = true;
    l._pzemErrorCount = PZEM_ERROR_THRESHOLD;
    CHECK(l.ok() == false);
}

TEST_CASE("ok(): false when SD not available even if PZEM ok") {
    Logger l;
    l._sdOk = false;
    l._pzemErrorCount = 0;
    CHECK(l.ok() == false);
}

TEST_CASE("ok(): true only when both PZEM ok and SD ok") {
    Logger l;
    l._sdOk = true;
    l._pzemErrorCount = 0;
    CHECK(l.ok() == true);
}

// ═══════════════════════════════════════════════════════════════════════════
// Sample struct
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("Sample struct: fields store and retrieve correctly") {
    Sample s { 12345, 230.1f, 1500.0f, 0.99f };
    CHECK(s.millis_ts  == 12345);
    CHECK(s.voltage_V  == doctest::Approx(230.1f));
    CHECK(s.power_W    == doctest::Approx(1500.0f));
    CHECK(s.pf         == doctest::Approx(0.99f));
}

