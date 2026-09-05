#include "SEDomains.h"
#include "core/FractalKernel.h"

constexpr uint32_t registerSystemHash = fnv1aHashConst("archetyped:system_execution:registerSystem");

void SEDomains::init() {
    FractalKernel::instance().registerCMDMethod(registerSystemHash, &SEDomains::registerSystemCMD);
}

void SEDomains::registerSystemCMD(FURCMDPacket& packet) {
    auto& ctx = *reinterpret_cast<registerSystemCMDContext*>(packet.payload);
    SystemExecution::instance().registerSystem([ctx]() {
        ctx.fn(ctx.context);
    }, ctx.frequencyMs);
}
