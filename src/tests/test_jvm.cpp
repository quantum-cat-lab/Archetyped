#include <iostream>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>

#include "../engine/components/JvmHost/JvmHost.h"
#include "../engine/components/JvmScheduler/JvmScheduler.h"
#include "../engine/components/JvmRegistry/JvmFactory.h"
#include "../engine/components/BridgeRegistry/BridgeRegistry.h"
#include "../engine/components/KotlinRuntime/KotlinFactory.h"
#include "../engine/components/SchemaRegistry/SchemaFactory.h"
#include "../engine/core/FractalKernel.h"
#include <SDK/archetyped/schema/SchemaBlock.h>
#include <SDK/archetyped/schema/SchemaRegistryPayload.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "[FAIL] " << msg << std::endl; ++g_fail; } \
    else { std::cout << "[PASS] " << msg << std::endl; ++g_pass; } \
} while(0)

static std::atomic<int> g_echoIn{0}, g_echoOut{0};
static void echoHandler(FURCMDPacket& pkt) {
    if (pkt.payload && pkt.payloadSize >= 4) {
        int v = *reinterpret_cast<int*>(pkt.payload);
        g_echoIn.store(v);
        if (pkt.outputBuffer) *reinterpret_cast<int*>(pkt.outputBuffer) = v + 1;
        g_echoOut.store(v + 1);
    }
}
extern "C" jint testAdd42(JNIEnv*, jclass, jint x) { return x + 42; }

int main() {
    std::cout << "=== test_jvm ===" << std::endl;

    FractalKernel::instance().init();
    JvmHost& host = JvmHost::instance();
    CHECK(host.init(), "JvmHost::init()");
    if (!host.isRunning()) {
        std::cerr << "[FATAL] VM did not start.\n";
        return 1;
    }

    JvmScheduler& sched = JvmScheduler::instance();
    CHECK(sched.init(), "JvmScheduler::init()");
    CHECK(sched.isRunning(), "JvmScheduler running");

    std::atomic<bool> done{false};
    std::atomic<bool> classOk{false};
    sched.submit(JvmTask{[&](JNIEnv* env) {
        jclass strCls = env->FindClass("java/lang/String");
        classOk.store(strCls != nullptr);
        if (env->ExceptionCheck()) JvmHost::hasException(env);
        done.store(true);
    }});
    for (int i = 0; i < 100 && !done.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(done.load(), "worker task executed");
    CHECK(classOk.load(), "FindClass(java/lang/String) from worker");

    FURCMDPacket regPkt{};
    JvmFactory::instance().registerJvmDomain(regPkt);
    uint32_t ctx = JvmFactory::instance().createContext("core", "./mods/kotlin/logic.jar");
    CHECK(ctx != 0, "JvmFactory::createContext('core')");

    SchemaRegisterSchemaPayload payload{};
    std::strncpy(payload.domain, "jvm", sizeof(payload.domain)-1);
    std::strncpy(payload.name, "Vec3", sizeof(payload.name)-1);
    payload.fieldCount = 3;
    payload.fields[0].typeId = SCHEMA_F32; std::strncpy(payload.fields[0].name, "x", 47);
    payload.fields[1].typeId = SCHEMA_F32; std::strncpy(payload.fields[1].name, "y", 47);
    payload.fields[2].typeId = SCHEMA_F32; std::strncpy(payload.fields[2].name, "z", 47);
    FURCMDPacket p{};
    p.methodHash = fnv1aHashConst("archetyped:jvm:registerStruct");
    p.payload = &payload;
    p.payloadSize = sizeof(payload);
    FractalKernel::instance().sendCMDPacket(p);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto* vec3 = SchemaFactory::instance().getSchema("jvm", "Vec3");
    CHECK(vec3 != nullptr, "SchemaFactory::getSchema('jvm','Vec3')");
    if (vec3) {
        CHECK(vec3->fields.size() == 3, "Vec3 has 3 fields");
        CHECK(vec3->fields[0].offset == 0 && vec3->fields[1].offset == 4 && vec3->fields[2].offset == 8, "Vec3 offsets 0,4,8");
        CHECK(vec3->size == 12, "Vec3 size 12");
        uint32_t offY = SchemaFactory::instance().getSchemaFieldOffset("jvm", vec3->hash, fnv1aHash("y"));
        CHECK(offY == 4, "getFieldOffset y==4");
    }

    auto blob = SchemaFactory::instance().dumpSchema("jvm");
    CHECK(!blob.empty(), "SchemaFactory::dumpSchema non-empty");
    if (!blob.empty()) {
        auto* sb = reinterpret_cast<SchemaBlock*>(blob.data());
        CHECK(sb->field_count == 3, "dump field_count 3");
        CHECK(sb->total_size == 12, "dump total_size 12");
    }

    CHECK(BridgeRegistry::jniSig(SCHEMA_I32) == "I", "BridgeRegistry jniSig I32->I");
    CHECK(BridgeRegistry::jniSig(SCHEMA_F32) == "F", "BridgeRegistry jniSig F32->F");
    {
        uint32_t args1[] = {SCHEMA_I32};
        CHECK(BridgeRegistry::methodSig(SCHEMA_I32, args1) == "(I)I", "BridgeRegistry methodSig (I)I");
        uint32_t args2[] = {SCHEMA_F32, SCHEMA_F32};
        CHECK(BridgeRegistry::methodSig(SCHEMA_VOID, args2) == "(FF)V", "BridgeRegistry methodSig (FF)V");
    }

    {
        uint32_t arg = SCHEMA_I32;
        BridgeRegistry::instance().installDirect(ctx, "java/lang/String", "dummyBridgeTestMethod999", SCHEMA_I32, std::span<const uint32_t>(&arg, 1), reinterpret_cast<void*>(&testAdd42));
        CHECK(true, "BridgeRegistry installDirect no crash");
    }

    {
        constexpr uint32_t kEcho = fnv1aHashConst("test:bridge:echo");
        FractalKernel::instance().registerCMDMethod(kEcho, echoHandler);
        int in = 41;
        int out = 0;
        std::atomic<uint64_t> fence{0};
        FURCMDPacket pkt{};
        pkt.methodHash = kEcho;
        pkt.payload = &in;
        pkt.payloadSize = sizeof(in);
        pkt.outputBuffer = &out;
        pkt.fence = reinterpret_cast<uint64_t*>(&fence);
        FractalKernel::instance().sendCMDPacket(pkt);
        for (int i = 0; i < 200 && fence.load() == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(fence.load() == 1, "FURCMD bridge fence signaled");
        CHECK(g_echoIn.load() == 41, "FURCMD bridge handler saw 41");
        CHECK(out == 42, "FURCMD bridge output 42");
        CHECK(g_echoOut.load() == 42, "FURCMD bridge echoOut 42");
    }

    {
        BridgeRegistry::instance().installFURCMDBridge(ctx, "java/lang/String");
        CHECK(true, "BridgeRegistry installFURCMDBridge no crash");
    }

    SchemaFactory::instance().invalidateDomain("jvm");
    CHECK(SchemaFactory::instance().getSchema("jvm", "Vec3") == nullptr, "invalidateDomain clears");

    {
        FURCMDPacket kp{};
        KotlinFactory::instance().registerDomain(kp);
        CHECK(true, "KotlinFactory registerDomain no crash");
        uint32_t kId = KotlinFactory::instance().registerContext("kTest", "./build", "java/lang/String");
        CHECK(kId != 0, "KotlinFactory registerContext kTest");
        CHECK(KotlinFactory::instance().hasContext("kTest"), "KotlinFactory hasContext kTest");
        CHECK(KotlinFactory::instance().findContext("kTest") == kId, "KotlinFactory findContext kTest");
        std::atomic<bool> kDone{false};
        KotlinFactory::instance().submit("kTest", [&](JNIEnv* env, jclass klass){
            kDone.store(klass != nullptr);
            (void)env;
        });
        for (int i=0;i<100 && !kDone.load();++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        CHECK(kDone.load(), "KotlinFactory submit sees klass");
        uint32_t kId2 = KotlinFactory::instance().registerContext("kTest", "./build", "java/lang/String");
        CHECK(kId2 == kId, "KotlinFactory duplicate register returns same id");
        CHECK(KotlinFactory::instance().unregisterContext("kTest"), "KotlinFactory unregister kTest");
        CHECK(!KotlinFactory::instance().hasContext("kTest"), "KotlinFactory hasContext false after unregister");
        CHECK(!KotlinFactory::instance().unregisterContext("kTest"), "KotlinFactory double unregister false");
    }

    sched.stop();
    host.shutdown();
    CHECK(!sched.isRunning(), "JvmScheduler stopped");
    CHECK(!host.isRunning(), "JvmHost stopped");

    std::cout << "\n=== JVM Test Summary ===" << std::endl;
    std::cout << "Passed: " << g_pass << "\nFailed: " << g_fail << std::endl;
    return g_fail == 0 ? 0 : 1;
}
