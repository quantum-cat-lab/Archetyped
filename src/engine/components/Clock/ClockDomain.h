#pragma once
#include "Clock.h"
#include "FURCMD/FURCMD.h"

class ClockDomain {
public:
    static void init();
    static void update();

    static void getDeltaTimeCMD(FURCMDPacket& packet);
    static void getTotalTimeCMD(FURCMDPacket& packet);
    static void getTimeCMD(FURCMDPacket& packet);

    static Clock& getClock();

private:
    static inline Clock s_clock;
};
