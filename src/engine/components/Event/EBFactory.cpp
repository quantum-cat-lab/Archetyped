#include "EBFactory.h"
#include "Engine/Engine.hpp"
#include "ankerl/unordered_dense.h"
#include "core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include "components/Event/EventInstance.h"
#include <cstdint>
#include <cstdio>
#include <unistd.h>
constexpr uint32_t registerEBHash = fnv1aHashConst("archetyped:event_bus:registerEBInstance");
constexpr uint32_t subscribeEventHash = fnv1aHashConst("archetyped:event_bus:subscribeEvent");
constexpr uint32_t emitEventHash = fnv1aHashConst("archetyped:event_bus:emitEvent");
constexpr uint32_t pushEventHash = fnv1aHashConst("archetyped:event_bus:pushEvent");
constexpr uint32_t processEventsHash = fnv1aHashConst("archetyped:event_bus:processEvents");
constexpr uint32_t resetEventsHash = fnv1aHashConst("archetyped:event_bus:resetEvents");
constexpr uint32_t unsubscribeEventHash = fnv1aHashConst("archetyped:event_bus:unsubscribeEvent");
ankerl::unordered_dense::map<uint32_t, EventInstance*, IdentityHash> EBFactory::eventBuses;
EBFactory::EBFactory() {
    FractalKernel::instance().registerCMDMethod(registerEBHash, &registerEBDomain);
    FractalKernel::instance().registerCMDMethod(subscribeEventHash, &subscribeEventCMD);
    FractalKernel::instance().registerCMDMethod(emitEventHash, &emitEventCMD);
    FractalKernel::instance().registerCMDMethod(pushEventHash, &pushEventCMD);
    FractalKernel::instance().registerCMDMethod(processEventsHash, &processEventsCMD);
    FractalKernel::instance().registerCMDMethod(resetEventsHash, &resetEventsCMD);
    FractalKernel::instance().registerCMDMethod(unsubscribeEventHash, &unsubscribeEventCMD);
}
void EBFactory::registerEBDomain(FURCMDPacket &packet) {
    auto* ctx = reinterpret_cast<uint32_t*>(packet.payload);
    uint32_t domainId = *ctx;
    if (eventBuses.find(domainId) != eventBuses.end()) {
        std::fprintf(stderr, "Domain %u already exists!\n", domainId);
        return;
    }
    auto* eb = new EventInstance();
    std::fprintf(stderr, "[EBFactory] Registered domain %u -> EventInstance %p\n", domainId, (void*)eb);
    eventBuses[domainId] = eb;

    // Update fence so sender can proceed
    if (packet.fence) {
        *packet.fence = 1;
    }
}
void EBFactory::emitEventCMD(FURCMDPacket &packet){
    emitEventInstanceCMDContext& context = *reinterpret_cast<emitEventInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = eventBuses.find(domainId);
    if (it != eventBuses.end()){
        it->second->emitEvent(context.eventId, context.data);
    }
}
void EBFactory::subscribeEventCMD(FURCMDPacket &packet){
    subscribeEventInstanceCMDContext& context = *reinterpret_cast<subscribeEventInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = eventBuses.find(domainId);
    if (it != eventBuses.end()){
        if (it->second == nullptr) {
            it->second = new EventInstance();
        }
        it->second->subscribe(context.eventId, context.cb, context.user, context.subscriberId);
    } else {
        eventBuses[domainId] = new EventInstance();
        eventBuses[domainId]->subscribe(context.eventId, context.cb, context.user, context.subscriberId);
    }
}
void EBFactory::pushEventCMD(FURCMDPacket &packet){
    pushEventInstanceCMDContext& context = *reinterpret_cast<pushEventInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = eventBuses.find(domainId);
    if (it != eventBuses.end()){
        it->second->pushEvent(context.eventId, context.data);
    }
}
void EBFactory::processEventsCMD(FURCMDPacket &packet){
    processEventsInstanceCMDContext& context = *reinterpret_cast<processEventsInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = eventBuses.find(domainId);
    if (it != eventBuses.end()){
        it->second->processEvents();
    }
}
void EBFactory::resetEventsCMD(FURCMDPacket &packet){
    resetEventsInstanceCMDContext& context = *reinterpret_cast<resetEventsInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = eventBuses.find(domainId);
    if (it != eventBuses.end()){
        it->second->reset();
    }
}
void EBFactory::unsubscribeEventCMD(FURCMDPacket &packet){
    unsubscribeEventInstanceCMDContext& context = *reinterpret_cast<unsubscribeEventInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;
    auto it = eventBuses.find(domainId);
    if (it != eventBuses.end()){
        it->second->unsubscribe(context.subscriberId);
    }
}
