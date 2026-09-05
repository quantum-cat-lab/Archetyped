#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <mutex>
#include <jni.h>
#include "ankerl/unordered_dense.h"
#include "FURCMD/FURCMD.h"

struct kotlinRegisterInstanceCtx {
    char domain[64];
    char jarPath[1024];
    char mainClass[256];
    uint32_t outContextId = 0;
};

struct kotlinUnregisterInstanceCtx {
    char domain[64];
};

struct KotlinInstance {
    uint32_t id = 0;
    std::string domain;
    std::string jarPath;
    std::string mainClass;
    jclass klass = nullptr;
};

class KotlinFactory {
public:
    static KotlinFactory& instance();

    static void registerDomain(FURCMDPacket& pkt);
    static void registerContextCMD(FURCMDPacket& pkt);
    static void unregisterContextCMD(FURCMDPacket& pkt);

    uint32_t registerContext(const char* domain, const char* jarPath, const char* mainClass);
    bool unregisterContext(const char* domain);
    bool unregisterContext(uint32_t domainHash);
    uint32_t findContext(const char* domain) const;
    bool hasContext(const char* domain) const;

    void submit(const char* domain, std::function<void(JNIEnv*, jclass)> task);
    void submit(uint32_t domainHash, std::function<void(JNIEnv*, jclass)> task);

    bool callStaticVoid(const char* domain, const char* method, const char* sig);

private:
    KotlinFactory() = default;
    KotlinFactory(const KotlinFactory&) = delete;
    KotlinFactory& operator=(const KotlinFactory&) = delete;

    static uint32_t hashDomain(const char* d);
    static std::string resolveJar(const char* jarPath);

    ankerl::unordered_dense::map<uint32_t, KotlinInstance> ctxs_;
    mutable std::mutex m_;
};
