#pragma once
#include "SystemExecution.h"
#include "FURCMD/FURCMD.h"
#include <cstdint>

typedef void (*system_update_fn)(void* context);
struct registerSystemCMDContext {
    system_update_fn fn;
    void* context;
    uint32_t frequencyMs;
};

class SEDomains {
public:
    static void init();
    static void registerSystemCMD(FURCMDPacket& packet);
};
