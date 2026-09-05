#pragma once
#include "FractalSDK.h"

namespace FractalSDK {
namespace Clock {

    /// Live monotonic seconds since engine start. Available at any moment,
    /// independent of the engine frame loop — modules use it to run their
    /// own tick loops (accumulator / fixed timestep).
    inline float getTime() {
        float result = 0.0f;
        getTimeContext ctx;

        FURCMDPacket packet;
        packet.methodHash = getTimeHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.outputBuffer = &result;

        Ticket* ticket = SDK::Get()->allocateTicket();
        packet.fence = (uint64_t*)&ticket->fence;

        SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
        return result;
    }

    inline float getDeltaTime() {
        float result = 0.0f;
        getDeltaTimeContext ctx;

        FURCMDPacket packet;
        packet.methodHash = getDeltaTimeHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.outputBuffer = &result;

        Ticket* ticket = SDK::Get()->allocateTicket();
        packet.fence = (uint64_t*)&ticket->fence;

        SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
        return result;
    }

    inline float getTotalTime() {
        float result = 0.0f;
        getTotalTimeContext ctx;

        FURCMDPacket packet;
        packet.methodHash = getTotalTimeHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.outputBuffer = &result;

        Ticket* ticket = SDK::Get()->allocateTicket();
        packet.fence = (uint64_t*)&ticket->fence;

        SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
        return result;
    }

}
}
