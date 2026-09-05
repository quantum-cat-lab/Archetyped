#pragma once
#include "FractalSDK.h"
#include <string>
#include <vector>
#include <cstring>

class WorkScheduler {
public:
    WorkScheduler(std::string name) {
        domainId = fnv1aHash(name);
        domainName = name;

        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        FURCMDPacket packet;
        packet.methodHash = registerWSDomainHash;
        packet.payloadSize = sizeof(uint32_t);
        packet.payload = &domainId;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    void schedule(w_task_fn fn, void* context) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        ComputeTask task;
        task.fn = fn;
        task.context = context;

        uint32_t payloadSize = sizeof(task) + sizeof(uint32_t);
        std::vector<uint8_t> buffer(payloadSize);
        
        memcpy(buffer.data(), &domainId, sizeof(uint32_t));
        memcpy(buffer.data() + sizeof(uint32_t), &task, sizeof(task));

        FURCMDPacket packet;
        packet.methodHash = scheduleTaskHash;
        packet.payloadSize = payloadSize;
        packet.payload = buffer.data();
        packet.fence = (uint64_t*)&ticket->fence;
        
        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    std::string getDomainName() {
        return domainName;
    }

    uint32_t getDomainHashId() {
        return domainId;
    }

private:
    uint32_t domainId;
    std::string domainName;
};