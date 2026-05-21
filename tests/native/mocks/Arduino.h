#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <cstdint>
#include <cmath>
#include <iostream>

#define VSPI 3
#define INPUT_PULLUP 2
#define INPUT 1

inline uint32_t millis() { return 0; }
inline void delay(uint32_t ms) {}

class MockSerial {
public:
    template<typename... Args>
    void printf(const char* format, Args... args) {}
    void println(const char* str) {}
};

extern MockSerial Serial;
extern MockSerial Serial2;

#endif
