#pragma once
#include "SmartScheduler.h"
#include "FURCMD/FURCMD.h"
#include <cstdint>

typedef void (*ss_task_fn)(void* context);
struct scheduleSSCMDContext {
    ss_task_fn fn;
    void* context;
    uint32_t value; // interval or delay
};

class SSDomains {
public:
    static void init();
    static void scheduleCyclicCMD(FURCMDPacket& packet);
    static void scheduleDelayedCMD(FURCMDPacket& packet);
    static void scheduleTickCMD(FURCMDPacket& packet);
};
