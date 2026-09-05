#pragma once
#include "EventInstance.h"
#include "FURCMD/FURCMD.h"
struct subscribeEventInstanceCMDContext {
    uint32_t domainId;
    uint32_t eventId;
    EventCallback cb;
    void* user;
    uint32_t subscriberId;
};
struct emitEventInstanceCMDContext {
    uint32_t domainId;
    uint32_t eventId;
    EventData data;
};
struct pushEventInstanceCMDContext {
    uint32_t domainId;
    uint32_t eventId;
    EventData data;
};
struct processEventsInstanceCMDContext {
    uint32_t domainId;
};
struct resetEventsInstanceCMDContext {
    uint32_t domainId;
};
struct unsubscribeEventInstanceCMDContext {
    uint32_t domainId;
    uint32_t subscriberId;
};


class EBFactory {
public:
    EBFactory();
    ~EBFactory() = default;
    static void registerEBDomain(FURCMDPacket& packet);
    static void subscribeEventCMD(FURCMDPacket& packet);
    static void emitEventCMD(FURCMDPacket& packet);
    static void pushEventCMD(FURCMDPacket& packet);
    static void processEventsCMD(FURCMDPacket& packet);
    static void resetEventsCMD(FURCMDPacket& packet);
    static void unsubscribeEventCMD(FURCMDPacket& packet);
private:
    static ankerl::unordered_dense::map<uint32_t, EventInstance*, IdentityHash> eventBuses;

};