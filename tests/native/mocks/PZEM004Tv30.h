#ifndef MOCK_PZEM004T_H
#define MOCK_PZEM004T_H

#include "Arduino.h"

class PZEM004Tv30 {
public:
    PZEM004Tv30(MockSerial& serial, int rx, int tx) {}
    float voltage() { return 230.0f; }
    float power() { return 1500.0f; }
    float pf() { return 0.98f; }
};

#endif
