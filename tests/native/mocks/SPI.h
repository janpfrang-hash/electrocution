#ifndef MOCK_SPI_H
#define MOCK_SPI_H

class SPIClass {
public:
    SPIClass(int bus) {}
    void begin(int clk, int miso, int mosi, int ss) {}
};

#endif
