#pragma once
#include "FURCMD/FURCMD.h"
#include <SDK/archetyped/hash/hash.h>
#include "workScheduler.h"
struct scheduleTaskInstanceCMDContext {
    uint32_t domainId;
    ComputeTask task;
};
class WSFactory {
public:
    WSFactory();
    ~WSFactory() = default;
    static void registerWSDomain(FURCMDPacket& packet);
    static void scheduleTaskCMD(FURCMDPacket& packet);
private:
    static ankerl::unordered_dense::map<uint32_t, ComputeInstance*, IdentityHash> computeSchedulers;
};