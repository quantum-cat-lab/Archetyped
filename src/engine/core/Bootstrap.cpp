#include "Bootstrap.h"
#include "FractalKernel.h"
#include "../components/Clock/ClockDomain.h"
#include "../components/ECS/Component/CMFactory.h"
#include "../components/Event/EBFactory.h"
#include "../components/ContainerLoader/ContainerLoader.h"
#include "../components/ModuleLoader/ModuleFactory.h"
#include "../components/vfs/VFSDomains.h"
#include "../components/workScheduler/WSFactory.h"
#include "../components/SmartScheduler/SSDomains.h"
#include "../components/SystemExecution/SEDomains.h"
#include "Engine/Engine.hpp"
#ifdef ARCHETYPED_ENABLE_CLR
#include "../components/ClrHost/ClrHost.h"
#include "../componen../CSharpRuntime/CSharpFactory.h"
#include "../componen../ClrRegistry/ClrFactory.h"
#endif
#ifdef ARCHETYPED_ENABLE_JVM
#include "../components/JvmHost/JvmHost.h"
#endif
#include <SDK/archetyped/hash/hash.h>
#include <iostream>
#include <cstring>
#include <chrono>
#include <poll.h>
#include <unistd.h>

Bootstrap& Bootstrap::instance() {
    static Bootstrap inst;
    return inst;
}

void Bootstrap::init() {
    if (inited_) return;
    FractalKernel::instance().init();
#ifdef ARCHETYPED_ENABLE_CLR
    if (ClrHost::instance().init()) {
        FURCMDPacket p{};
        CSharpFactory::registerDomain(p);
        ClrFactory::registerClrDomain(p);
    }
#endif
    arche::Engine engine;
    engine.init();

    CMFactory cmd;
    WSFactory wsd;
    EBFactory ebd;
    VFSDomains vfsd;
    SSDomains::init();
    SEDomains::init();
    ClockDomain::init();


    static ModuleFactory moduleLoader;
    static ContainerLoader containerLoader;
    uint32_t domainId = 0;
    FURCMDPacket regCM{ fnv1aHashConst("archetyped:ecs:registerCMInstance"), sizeof(domainId), 0, &domainId };
    FractalKernel::instance().sendCMDPacket(regCM);

    inited_ = true;
    std::cout << "[Bootstrap] init done\n";
}

bool Bootstrap::tick() {
    ClockDomain::update();
    FURCMDPacket pkt{ fnv1aHashConst("archetyped:vfs:watchDrain"), 0, 0, nullptr };
    FractalKernel::instance().sendCMDPacket(pkt);
    return true;
}

bool Bootstrap::shouldExit() {
    bool inputReady = std::cin.rdbuf()->in_avail() > 0;
    if (!inputReady) {
        struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
        inputReady = poll(&pfd, 1, 0) > 0;
    }
    if (inputReady) {
        std::string cmd;
        if (!(std::cin >> cmd) || cmd == "exit") return true;
    }
    return false;
}

void Bootstrap::shutdown() {
    SmartScheduler::instance().stop();
#ifdef ARCHETYPED_ENABLE_CLR
    ClrHost::instance().shutdown();
#endif
    inited_ = false;
    std::cout << "[Bootstrap] shutdown\n";
}
