#include <iostream>
#include <atomic>
#include <cassert>
#include "../engine/components/ClrHost/ClrHost.h"
#include "../engine/components/ClrScheduler/ClrScheduler.h"
#include "../engine/components/ClrFactory/ClrFactory.h"
#include "../engine/components/CSharpFactory/CSharpFactory.h"
#include "../engine/core/FractalKernel.h"

int main(){
    std::cout<<"=== test_clr ===\n";
    int passed=0, failed=0;
    auto CHECK=[&](bool ok, const char* msg){ if(ok){ std::cout<<"[PASS] "<<msg<<"\n"; ++passed;} else { std::cout<<"[FAIL] "<<msg<<"\n"; ++failed; } };

    FractalKernel::instance().init();
    CHECK(ClrHost::instance().init(), "ClrHost::init()");
    CHECK(ClrHost::instance().isRunning(), "ClrHost running");
    CHECK(ClrHost::instance().getLoadAssemblyFn()!=nullptr, "getLoadAssemblyFn");

    auto& sched = ClrScheduler::instance();
    CHECK(sched.init(), "ClrScheduler init");
    CHECK(sched.isRunning(), "ClrScheduler running");
    std::atomic<int> ctr{0};
    sched.submitAndWait(ClrTask{[&]{ ctr.fetch_add(1); }});
    CHECK(ctr.load()==1, "ClrScheduler submitAndWait");

    auto& reg = ClrFactory::instance();
    FURCMDPacket pkt{}; ClrFactory::registerClrDomain(pkt);
    uint32_t cid = reg.createContext("test.domain", "/tmp/fake.dll");
    CHECK(cid!=0, "ClrFactory createContext");
    CHECK(reg.getDllPath(cid)=="/tmp/fake.dll", "ClrFactory getDllPath");
    std::atomic<bool> ran{false};
    reg.submit(cid, [&]{ ran.store(true); });
    for(int i=0;i<50 && !ran.load();++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(ran.load(), "ClrFactory submit");

    auto& cs = CSharpFactory::instance();
    FURCMDPacket pkt2{}; CSharpFactory::registerDomain(pkt2);
    uint32_t csId = cs.registerContext("cTest", "mod://CSharpTest.dll", "CSharpTest.Entry");
    CHECK(csId!=0, "CSharpFactory registerContext");
    CHECK(cs.hasContext("cTest"), "CSharpFactory hasContext");
    uint32_t dup = cs.registerContext("cTest", "mod://CSharpTest.dll", "CSharpTest.Entry");
    CHECK(dup==csId, "CSharpFactory duplicate same id");
    std::atomic<bool> csRan{false};
    cs.submit("cTest", [&]{ csRan.store(true); });
    for(int i=0;i<50 && !csRan.load();++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(csRan.load(), "CSharpFactory submit");
    CHECK(cs.unregisterContext("cTest"), "CSharpFactory unregister");
    CHECK(!cs.hasContext("cTest"), "CSharpFactory gone after unregister");
    CHECK(!cs.unregisterContext("cTest"), "CSharpFactory double unregister false");

    sched.stop(); CHECK(!sched.isRunning(), "ClrScheduler stopped");
    ClrHost::instance().shutdown(); CHECK(!ClrHost::instance().isRunning(), "ClrHost shutdown");

    std::cout<<"\n=== CLR Test Summary ===\nPassed: "<<passed<<"\nFailed: "<<failed<<"\n";
    return failed==0?0:1;
}
