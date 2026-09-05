#pragma once
#include "FractalSDK.h"
#include "IKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstdint>

class VaultInstance {
public:
    VaultInstance(uint32_t domainId, uint32_t registryId = 0xFFFFFFFF)
        : m_domainId(domainId)
    {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        createVaultCMDContext ctx{ domainId, registryId };
        FURCMDPacket packet;
        packet.methodHash = createVaultHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.fence = (uint64_t*)&ticket->fence;
        FractalSDK::SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}

        ticket = FractalSDK::SDK::Get()->allocateTicket();
        registerDomainWithVaultCMDContext rctx{ domainId, domainId };
        FURCMDPacket rpacket;
        rpacket.methodHash = registerDomainWithVaultHash;
        rpacket.payloadSize = sizeof(rctx);
        rpacket.payload = &rctx;
        rpacket.fence = (uint64_t*)&ticket->fence;
        FractalSDK::SDK::Get()->sendPacket(rpacket);
        while (!ticket->isReady()) {}
    }

    void registerTable(uint32_t componentId, uint32_t capacity) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        registerTableFromRegistryCMDContext ctx{ m_domainId, componentId, capacity };
        FURCMDPacket packet;
        packet.methodHash = registerTableFromRegistryHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.fence = (uint64_t*)&ticket->fence;
        FractalSDK::SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
    }

    void linkTable(uint32_t componentId, uint32_t sourceDomainId) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        linkComponentTableCMDContext ctx{ m_domainId, componentId, sourceDomainId };
        FURCMDPacket packet;
        packet.methodHash = linkComponentTableHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;
        packet.fence = (uint64_t*)&ticket->fence;
        FractalSDK::SDK::Get()->sendPacket(packet);
        while (!ticket->isReady()) {}
    }

    uint32_t getDomainId() const { return m_domainId; }

private:
    uint32_t m_domainId;
};
