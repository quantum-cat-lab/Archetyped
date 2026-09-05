#pragma once
#include "FractalSDK.h"
#include <cstdint>
#include <vector>
#include <cstring>

namespace FractalSDK {
namespace SmartScheduler {

    inline void scheduleCyclic(ss_task_fn fn, void* context, uint32_t intervalMs) {
        scheduleSSCMDContext ctx;
        ctx.fn = fn;
        ctx.context = context;
        ctx.value = intervalMs;

        FURCMDPacket packet;
        packet.methodHash = scheduleCyclicHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        SDK::Get()->sendPacket(packet);
    }

    inline void scheduleDelayed(ss_task_fn fn, void* context, uint32_t delayMs) {
        scheduleSSCMDContext ctx;
        ctx.fn = fn;
        ctx.context = context;
        ctx.value = delayMs;

        FURCMDPacket packet;
        packet.methodHash = scheduleDelayedHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        SDK::Get()->sendPacket(packet);
    }

    inline void scheduleTick(ss_task_fn fn, void* context) {
        scheduleSSCMDContext ctx;
        ctx.fn = fn;
        ctx.context = context;
        ctx.value = 0;

        FURCMDPacket packet;
        packet.methodHash = scheduleTickHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        SDK::Get()->sendPacket(packet);
    }

}
}
