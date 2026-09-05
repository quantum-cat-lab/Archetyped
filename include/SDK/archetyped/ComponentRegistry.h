#pragma once
#include "FractalSDK.h"
#include "IKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstdint>

class ComponentRegistry {
public:
    ComponentRegistry(uint32_t registryId) : m_registryId(registryId) {}

    void create() {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        createRegistryCMDContext ctx{ m_registryId };
        FURCMDPacket packet;
        packet.methodHash = createRegistryHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.fence = (uint64_t*)&ticket->fence;
        FractalSDK::SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
    }

    void registerType(uint32_t componentId, uint32_t componentSize, uint32_t alignment = 16) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        registerTypeCMDContext ctx{ m_registryId, componentId, componentSize, alignment };
        FURCMDPacket packet;
        packet.methodHash = registerTypeHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.fence = (uint64_t*)&ticket->fence;
        FractalSDK::SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
    }

    uint32_t getId() const { return m_registryId; }

private:
    uint32_t m_registryId;
};
