#include "ClockDomain.h"
#include "core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>

constexpr uint32_t getDeltaTimeHash = fnv1aHashConst("archetyped:clock:getDeltaTime");
constexpr uint32_t getTotalTimeHash = fnv1aHashConst("archetyped:clock:getTotalTime");
constexpr uint32_t getTimeHash      = fnv1aHashConst("archetyped:clock:getTime");

void ClockDomain::init() {
    FractalKernel::instance().registerCMDMethod(getDeltaTimeHash, &ClockDomain::getDeltaTimeCMD);
    FractalKernel::instance().registerCMDMethod(getTotalTimeHash, &ClockDomain::getTotalTimeCMD);
    FractalKernel::instance().registerCMDMethod(getTimeHash, &ClockDomain::getTimeCMD);
}

void ClockDomain::update() {
    s_clock.update();
}

void ClockDomain::getDeltaTimeCMD(FURCMDPacket& packet) {
    float dt = s_clock.getDeltaTime();
    if (packet.outputBuffer) {
        *reinterpret_cast<float*>(packet.outputBuffer) = dt;
    }
}

void ClockDomain::getTotalTimeCMD(FURCMDPacket& packet) {
    float tt = s_clock.getTotalTime();
    if (packet.outputBuffer) {
        *reinterpret_cast<float*>(packet.outputBuffer) = tt;
    }
}

void ClockDomain::getTimeCMD(FURCMDPacket& packet) {
    float t = static_cast<float>(Clock::nowSeconds());
    if (packet.outputBuffer) {
        *reinterpret_cast<float*>(packet.outputBuffer) = t;
    }
}

Clock& ClockDomain::getClock() {
    return s_clock;
}
