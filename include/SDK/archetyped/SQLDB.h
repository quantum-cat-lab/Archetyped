#pragma once
#include "FractalSDK.h"
#include <string>

class SQLDB {
public:
    SQLDB(std::string name) {
        domainId = fnv1aHash(name);
        domainName = name;

        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        registerSQLDomainContext context;
        context.domainId = domainId;
        context.domainName = domainName.c_str();

        FURCMDPacket packet;
        packet.methodHash = registerSQLDomainHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    void open(const char* dbPath) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        openInstanceCMDContext context;
        context.domainId = domainId;
        context.dbPath = dbPath;

        FURCMDPacket packet;
        packet.methodHash = openCMDHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    void execute(const char* sql) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        executeInstanceCMDContext context;
        context.domainId = domainId;
        context.sql = sql;

        FURCMDPacket packet;
        packet.methodHash = executeCMDHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    void setString(const char* key, const char* value) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        setStringInstanceCMDContext context;
        context.domainId = domainId;
        context.key = key;
        context.value = value;

        FURCMDPacket packet;
        packet.methodHash = setStringCMDHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
    }

    bool exists(const char* key) {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        existsInstanceCMDContext context;
        context.domainId = domainId;
        context.key = key;
        bool result = false;

        FURCMDPacket packet;
        packet.methodHash = existsCMDHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
        packet.outputBuffer = &result;
        packet.fence = (uint64_t*)&ticket->fence;

        FractalSDK::SDK::Get()->sendPacket(packet);
        while(!ticket->isReady()) {}
        return result;
    }

    void close() {
        Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
        closeInstanceCMDContext context;
        context.domainId = domainId;

        FURCMDPacket packet;
        packet.methodHash = closeCMDHash;
        packet.payloadSize = sizeof(context);
        packet.payload = &context;
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