#pragma once
#include "FractalSDK.h"
#include <cstdint>

namespace FractalSDK {
namespace SystemExecution {

    inline void registerSystem(system_update_fn fn, void* context, uint32_t frequencyMs = 0) {
        registerSystemCMDContext ctx;
        ctx.fn = fn;
        ctx.context = context;
        ctx.frequencyMs = frequencyMs;

        FURCMDPacket packet;
        packet.methodHash = registerSystemHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        SDK::Get()->sendPacket(packet);
    }

}
}
