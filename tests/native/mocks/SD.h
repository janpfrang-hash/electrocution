#ifndef MOCK_SD_H
#define MOCK_SD_H

#include <cstdint>
#include <cstddef>
#include "SPI.h"

class File {
public:
    operator bool() const { return true; }
    void close() {}
    size_t write(const uint8_t* buf, size_t size) { return size; }
    size_t println(const char* str) { return 0; }
};

class SDClass {
public:
    bool begin(int cs, SPIClass& spi, uint32_t speed) { return true; }
    void end() {}
    File open(const char* path, const char* mode = "r") { return File(); }
    bool remove(const char* path) { return true; }
};

extern SDClass SD;

#endif
