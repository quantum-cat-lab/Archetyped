#pragma once
#include <jni.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

struct JvmConfig {
    std::vector<std::string> opts = {
        "--enable-native-access=ALL-UNNAMED",
        "-Xmx512m",
    };
    std::string classpath = "./mods";
};

class JvmHost {
public:
    static JvmHost& instance();

    bool init(const JvmConfig& cfg = {});
    void shutdown();

    bool isRunning() const { return running_.load(); }
    JavaVM* vm() const { return vm_; }
    JNIEnv* env() const;
    JNIEnv* attachThread();
    void detachThread();
    void* libHandle() const { return libHandle_; }

    static bool hasException(JNIEnv* env);

private:
    JvmHost() = default;
    ~JvmHost() { shutdown(); }
    JvmHost(const JvmHost&) = delete;
    JvmHost& operator=(const JvmHost&) = delete;

    // resolve libjvm.so: ./runtime/lib/server (or bin/server on win) else JAVA_HOME
    static void* openRuntimeLib();
    static const char* runtimeLibName();

    JavaVM* vm_ = nullptr;
    void* libHandle_ = nullptr;

    std::atomic<bool> running_{false};

    JvmConfig cfg_;
};
