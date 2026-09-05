#pragma once
#include "FractalSDK.h"
#include "IKernel.h"
#include <cstdint>
#include <cstring>
#include <string>

namespace FractalSDK {
namespace ModuleLoader {

    inline void loadModule(const char* path, const ModuleConfig& config) {
        loadModuleContext ctx;
        ctx.path = path;

        FURCMDPacket packet;
        packet.methodHash = loadModuleHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        SDK::Get()->sendPacket(packet);
    }

    inline bool isLoaded(const char* path) {
        isLoadedCMDContext ctx;
        ctx.path = path;

        bool result = false;
        FURCMDPacket packet;
        packet.methodHash = isLoadedHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.outputBuffer = &result;

        Ticket* ticket = SDK::Get()->allocateTicket();
        packet.fence = (uint64_t*)&ticket->fence;

        SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
        return result;
    }

    inline void setState(const char* path, int state) {
        setModuleStateContext ctx;
        ctx.path = path;
        ctx.state = state;

        FURCMDPacket packet;
        packet.methodHash = setModuleStateHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        SDK::Get()->sendPacket(packet);
    }

    inline int getState(const char* path) {
        getModuleStateContext ctx;
        ctx.path = path;

        int result = -1;
        FURCMDPacket packet;
        packet.methodHash = getModuleStateHash;
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
