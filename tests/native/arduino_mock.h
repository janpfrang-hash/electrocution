/*
 * arduino_mock.h
 * Minimal stub of the Arduino HAL so Logger logic can compile and run
 * on a standard Linux host (no ESP32 toolchain needed).
 *
 * Only the symbols actually used by the code under test are provided.
 */
#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <chrono>

// ── Time ────────────────────────────────────────────────────────────────────
inline uint32_t millis() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - start).count();
}
inline void delay(uint32_t) {}

// ── Math ────────────────────────────────────────────────────────────────────
#ifndef NAN
#define NAN  __builtin_nanf("")
#endif
using std::isnan;

// ── Serial stub ─────────────────────────────────────────────────────────────
struct SerialStub {
    template<typename T> void print(T)   {}
    template<typename T> void println(T) {}
    void println()                        {}
    void printf(const char*, ...)         {}
    void begin(int)                       {}
} Serial;

// ── HardwareSerial stub ──────────────────────────────────────────────────────
struct HardwareSerial {
    HardwareSerial(int) {}
} Serial2(2);

// ── SPI stub ────────────────────────────────────────────────────────────────
#define VSPI 0
struct SPIClass {
    SPIClass(int) {}
    void begin(int,int,int,int) {}
};

// ── SD / FS stubs ────────────────────────────────────────────────────────────
#define FILE_APPEND 0
#define FILE_WRITE  1
#define FILE_READ   2

struct File {
    operator bool() const { return false; }
    void println(const char*) {}
    void printf(const char*, ...) {}
    void flush() {}
    void close() {}
};

struct SDClass {
    bool _ok = false;
    bool begin(int, SPIClass&, uint32_t) { return _ok; }
    void end() {}
    bool exists(const char*) { return false; }
    bool remove(const char*) { return true; }
    File open(const char*, int) { return File(); }
} SD;

// ── PZEM004Tv30 stub ─────────────────────────────────────────────────────────
struct PZEM004Tv30 {
    float _voltage = NAN;
    float _power   = NAN;
    float _pf      = NAN;

    PZEM004Tv30(HardwareSerial&, int, int) {}

    float voltage() { return _voltage; }
    float power()   { return _power;   }
    float pf()      { return _pf;      }

    // Test helpers
    void setReadings(float v, float p, float pf) {
        _voltage = v; _power = p; _pf = pf;
    }
    void setNaN() {
        _voltage = NAN; _power = NAN; _pf = NAN;
    }
};

// ── Pin / GPIO stubs ─────────────────────────────────────────────────────────
#define INPUT_PULLUP 0
#define INPUT        1
#define OUTPUT       2
#define HIGH         1
#define LOW          0
inline void pinMode(int,int)      {}
inline void digitalWrite(int,int) {}
inline int  analogRead(int)       { return 0; }
inline void analogReadResolution(int) {}

