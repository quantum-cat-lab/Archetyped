#include "JvmHost.h"

#include <cstdio>

#ifdef __linux__
#include <dlfcn.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

static thread_local JNIEnv* t_env = nullptr;

JvmHost& JvmHost::instance() {
    static JvmHost inst;
    return inst;
}

JNIEnv* JvmHost::env() const {
    JNIEnv* e = nullptr;
    if (vm_ && vm_->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_10) == JNI_OK)
        return e;
    return nullptr;
}

bool JvmHost::hasException(JNIEnv* env) {
    if (!env || !env->ExceptionCheck()) return false;
    env->ExceptionDescribe();
    env->ExceptionClear();
    return true;
}

const char* JvmHost::runtimeLibName() {
#ifdef _WIN32
    return "jvm.dll";
#else
    return "libjvm.so";
#endif
}

void* JvmHost::openRuntimeLib() {
#ifdef ARCHETYPED_JVM_VENDORED
    // search order: ./runtime/{lib/server,jvm}, JAVA_HOME, system
    const char* candidates[] = {
        "./runtime/lib/server/libjvm.so",
        "./runtime/lib/server/jvm.dll",
        "./runtime/bin/server/jvm.dll",
        nullptr,
    };
    const char* env = std::getenv("JAVA_HOME");
    char javaHomePath[1024];
    if (env) {
        std::snprintf(javaHomePath, sizeof(javaHomePath),
#ifdef _WIN32
                      "%s\\bin\\server\\jvm.dll", env);
#else
                      "%s/lib/server/libjvm.so", env);
#endif
        candidates[2] = javaHomePath;
    }
    for (int i = 0; candidates[i]; ++i) {
#ifdef __linux__
        void* h = dlopen(candidates[i], RTLD_LAZY | RTLD_GLOBAL);
        if (h) return h;
#elif defined(_WIN32)
        HMODULE h = LoadLibraryA(candidates[i]);
        if (h) return h;
#endif
    }
    std::fprintf(stderr, "[JvmHost] runtime lib not found (set JAVA_HOME or ship ./runtime)\n");
    return nullptr;
#else
    return nullptr; // linked directly via -ljvm
#endif
}

bool JvmHost::init(const JvmConfig& cfg) {
    if (running_.load()) return true;
    cfg_ = cfg;

#if defined(ARCHETYPED_JVM_VENDORED) && defined(__linux__)
    libHandle_ = openRuntimeLib();
    if (!libHandle_) return false;
#endif

    std::vector<std::string> owned;
    owned.reserve(cfg_.opts.size() + 1);
    for (auto& o : cfg_.opts) owned.push_back(o);
    if (!cfg_.classpath.empty())
        owned.push_back("-Djava.class.path=" + cfg_.classpath);

    std::vector<JavaVMOption> opts;
    opts.reserve(owned.size());
    for (auto& s : owned) {
        JavaVMOption o{};
        o.optionString = const_cast<char*>(s.c_str());
        opts.push_back(o);
    }

    JavaVMInitArgs args{};
    args.version = JNI_VERSION_10;
    args.nOptions = static_cast<jint>(opts.size());
    args.options = opts.data();
    args.ignoreUnrecognized = JNI_FALSE;

    JNIEnv* env = nullptr;
    jint rc = JNI_CreateJavaVM(&vm_, reinterpret_cast<void**>(&env), &args);
    if (rc == JNI_EEXIST) {
        jsize n = 0;
        JNI_GetCreatedJavaVMs(&vm_, 1, &n);
        if (n > 0 && vm_) {
            vm_->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr);
            t_env = env;
            rc = JNI_OK;
        }
    } else if (rc == JNI_OK) {
        t_env = env;
    }

    if (rc != JNI_OK || !vm_) {
        std::fprintf(stderr, "[JvmHost] JNI_CreateJavaVM failed: %d\n", rc);
        vm_ = nullptr;
        return false;
    }

    running_.store(true);
    std::fprintf(stderr, "[JvmHost] started classpath=%s workers-config-ready\n",
                 cfg_.classpath.c_str());
    return true;
}

void JvmHost::shutdown() {
    if (!running_.exchange(false)) return;

    if (vm_) {
        // detach current thread if attached
        JNIEnv* e = nullptr;
        if (vm_->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_10) == JNI_OK)
            vm_->DetachCurrentThread();
        vm_->DestroyJavaVM();
        vm_ = nullptr;
    }
    t_env = nullptr;

#if defined(ARCHETYPED_JVM_VENDORED) && defined(__linux__)
    if (libHandle_) { dlclose(libHandle_); libHandle_ = nullptr; }
#elif defined(ARCHETYPED_JVM_VENDORED) && defined(_WIN32)
    if (libHandle_) { FreeLibrary(static_cast<HMODULE>(libHandle_)); libHandle_ = nullptr; }
#endif

    std::fprintf(stderr, "[JvmHost] shutdown\n");
}

JNIEnv* JvmHost::attachThread() {
    if (!vm_) return nullptr;
    JNIEnv* env = nullptr;
    if (vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_10) == JNI_OK) {
        t_env = env;
        return env;
    }
    JavaVMAttachArgs args{};
    args.version = JNI_VERSION_10;
    args.name = const_cast<char*>("arche-worker");
    args.group = nullptr;
    if (vm_->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), &args) != JNI_OK)
        return nullptr;
    t_env = env;
    return env;
}

void JvmHost::detachThread() {
    if (vm_) vm_->DetachCurrentThread();
    t_env = nullptr;
}
