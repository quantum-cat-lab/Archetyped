#include "SSDomains.h"
#include "core/FractalKernel.h"
#include <iostream>

constexpr uint32_t scheduleCyclicHash = fnv1aHashConst("archetyped:smart_scheduler:scheduleCyclic");
constexpr uint32_t scheduleDelayedHash = fnv1aHashConst("archetyped:smart_scheduler:scheduleDelayed");
constexpr uint32_t scheduleTickHash = fnv1aHashConst("archetyped:smart_scheduler:scheduleTick");

void SSDomains::init() {
    FractalKernel::instance().registerCMDMethod(scheduleCyclicHash, &SSDomains::scheduleCyclicCMD);
    FractalKernel::instance().registerCMDMethod(scheduleDelayedHash, &SSDomains::scheduleDelayedCMD);
    FractalKernel::instance().registerCMDMethod(scheduleTickHash, &SSDomains::scheduleTickCMD);
    SmartScheduler::instance().run();
}

void SSDomains::scheduleCyclicCMD(FURCMDPacket& packet) {
    auto& ctx = *reinterpret_cast<scheduleSSCMDContext*>(packet.payload);
    SmartScheduler::instance().scheduleCyclic([ctx]() {
        ctx.fn(ctx.context);
    }, ctx.value);
}

void SSDomains::scheduleDelayedCMD(FURCMDPacket& packet) {
    auto& ctx = *reinterpret_cast<scheduleSSCMDContext*>(packet.payload);
    SmartScheduler::instance().scheduleDelayed([ctx]() {
        ctx.fn(ctx.context);
    }, ctx.value);
}

void SSDomains::scheduleTickCMD(FURCMDPacket& packet) {
    auto& ctx = *reinterpret_cast<scheduleSSCMDContext*>(packet.payload);
    SmartScheduler::instance().scheduleTick([ctx]() {
        ctx.fn(ctx.context);
    });
}
