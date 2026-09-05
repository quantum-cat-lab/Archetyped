#pragma once
#include "../JvmScheduler/JvmScheduler.h"
#include "../SchemaRegistry/SchemaInstance.h"
#include "FURCMD/FURCMD.h"

#include <jni.h>

#include <mutex>
#include <string>
#include <vector>
#include <memory>

#include "ankerl/unordered_dense.h"

// ---- FURCMD payload structs (C-API for modules, no jni.h on caller side) ----
struct jvmCreateInstanceCtx {
    char name[64];
    char jarPath[1024];
    uint32_t outContextId; // filled by handler
};
struct jvmDestroyInstanceCtx {
    uint32_t contextId;
};
struct jvmComposeInstanceCtx {
    uint32_t targetId;
    uint32_t importId;
};
struct jvmRegisterNativeInstanceCtx {
    uint32_t contextId;
    char className[256];
    char methodName[64];
    char signature[128];
    void* fnPtr;
};
struct jvmCallInstanceCtx {
    uint32_t contextId;
    char className[256];
    char methodName[64];
    char signature[128];
};

// ---- JVM context (one isolated ClassLoader + import list + per-domain SchemaInstance) ----
struct JvmInstance {
    uint32_t id;
    std::string name;
    jobject classLoader = nullptr; // GlobalRef URLClassLoader
    std::vector<uint32_t> imports; // context ids resolvable from this one
    std::unique_ptr<SchemaInstance> schemaReg; // per-domain schema (mirrors ClrFactory)
};

class JvmFactory {
public:
    static JvmFactory& instance();

    // FURCMD handlers (called from FURCMD worker thread)
    static void registerJvmDomain(FURCMDPacket& pkt);
    static void createContextCMD(FURCMDPacket& pkt);
    static void destroyContextCMD(FURCMDPacket& pkt);
    static void composeCMD(FURCMDPacket& pkt);
    static void registerNativeCMD(FURCMDPacket& pkt);
    static void registerSchemaCMD(FURCMDPacket& pkt);
    static void dumpSchemaCMD(FURCMDPacket& pkt);

    // Internal helpers (run inside JvmScheduler worker via env)
    uint32_t createContext(const char* name, const char* jarPath);
    void destroyContext(uint32_t id);
    void compose(uint32_t target, uint32_t importId);
    void registerNative(uint32_t ctxId, const char* cls, const char* method,
                        const char* sig, void* fnPtr);
    void registerSchema(uint32_t ctxId, const char* name,
                   const std::vector<std::pair<uint32_t, std::string>>& fields);

    // Resolve a class: search own loader, then imports (no transitive)
    jclass findClass(JNIEnv* env, uint32_t ctxId, const char* name);

    // Submit a task bound to a context (uses shared scheduler pool)
    void submit(uint32_t ctxId, std::function<void(JNIEnv*)> task);

private:
    JvmFactory() = default;
    JvmFactory(const JvmFactory&) = delete;
    JvmFactory& operator=(const JvmFactory&) = delete;

    jobject makeUrlClassLoader(JNIEnv* env, const char* jarPath);

    ankerl::unordered_dense::map<uint32_t, JvmInstance> ctxs_;
    std::mutex m_;
    JvmScheduler& sched_ = JvmScheduler::instance(); // singleton pool

    static constexpr uint32_t kCoreCtx = 0xABCDEF01; // "core" default import target
};