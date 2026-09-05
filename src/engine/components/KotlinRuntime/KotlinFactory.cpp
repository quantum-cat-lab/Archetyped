#include "KotlinFactory.h"
#include "../JvmRegistry/JvmFactory.h"
#include "../JvmScheduler/JvmScheduler.h"
#include "../JvmHost/JvmHost.h"
#include "../SchemaRegistry/SchemaFactory.h"
#include "../vfs/VFS.hpp"
#include "../../core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstdio>
#include <cstring>
#include <atomic>

namespace {
constexpr uint32_t kRegisterHash   = fnv1aHashConst("archetyped:kotlin:registerInstance");
constexpr uint32_t kUnregisterHash = fnv1aHashConst("archetyped:kotlin:unregisterInstance");
constexpr uint32_t kDomainHash     = fnv1aHashConst("archetyped:kotlin:registerInstance");
}

KotlinFactory& KotlinFactory::instance() {
    static KotlinFactory inst;
    return inst;
}

uint32_t KotlinFactory::hashDomain(const char* d) { return fnv1aHash(d); }

std::string KotlinFactory::resolveJar(const char* jarPath) {
    if (!jarPath || !jarPath[0]) return {};
    if (std::strstr(jarPath, "://")) {
        char out[VFS::MAX_PATH_LENGTH]{};
        if (VFS::Resolve(jarPath, out, sizeof(out))) return std::string(out);
        if (VFS::ResolveFirstExisting(jarPath, out, sizeof(out))) return std::string(out);
        return std::string(jarPath);
    }
    return std::string(jarPath);
}

void KotlinFactory::registerDomain(FURCMDPacket& pkt) {
    (void)pkt;
    FractalKernel::instance().registerCMDMethod(kRegisterHash, registerContextCMD);
    FractalKernel::instance().registerCMDMethod(kUnregisterHash, unregisterContextCMD);
    std::fprintf(stderr, "[KotlinFactory] domain registered\n");
}

void KotlinFactory::registerContextCMD(FURCMDPacket& pkt) {
    auto* c = static_cast<kotlinRegisterInstanceCtx*>(pkt.payload);
    if (!c) return;
    c->outContextId = instance().registerContext(c->domain, c->jarPath, c->mainClass);
}

void KotlinFactory::unregisterContextCMD(FURCMDPacket& pkt) {
    auto* c = static_cast<kotlinUnregisterInstanceCtx*>(pkt.payload);
    if (!c) return;
    instance().unregisterContext(c->domain);
}

uint32_t KotlinFactory::registerContext(const char* domain, const char* jarPath, const char* mainClass) {
    if (!domain || !domain[0]) return 0;
    uint32_t id = hashDomain(domain);
    {
        std::lock_guard<std::mutex> lk(m_);
        if (ctxs_.find(id) != ctxs_.end()) return id;
    }

    std::string resolved = resolveJar(jarPath ? jarPath : "");
    const char* jar = resolved.empty() ? jarPath : resolved.c_str();
    const char* klassName = (mainClass && mainClass[0]) ? mainClass : nullptr;

    uint32_t ctxId = JvmFactory::instance().createContext(domain, jar ? jar : "");
    if (!ctxId) return 0;

    jclass global = nullptr;
    if (klassName) {
        std::atomic<bool> done{false};
        std::atomic<bool> ok{false};
        JvmScheduler::instance().submitAndWait(JvmTask{[&](JNIEnv* env){
            jclass c = JvmFactory::instance().findClass(env, ctxId, klassName);
            if (c) {
                global = static_cast<jclass>(env->NewGlobalRef(c));
                ok.store(global != nullptr);
                if (env->ExceptionCheck()) JvmHost::hasException(env);
            } else {
                std::fprintf(stderr, "[KotlinFactory] findClass failed: %s (ctx %08x)\n", klassName, ctxId);
            }
            done.store(true);
        }});
        if (!ok.load()) {
            std::fprintf(stderr, "[KotlinFactory] warning: domain '%s' has no mainClass %s\n", domain, klassName);
        }
    }

    {
        std::lock_guard<std::mutex> lk(m_);
        KotlinInstance kc{};
        kc.id = id;
        kc.domain = domain;
        kc.jarPath = resolved.empty() ? (jarPath ? jarPath : "") : resolved;
        kc.mainClass = klassName ? klassName : "";
        kc.klass = global;
        ctxs_[id] = std::move(kc);
    }
    std::fprintf(stderr, "[KotlinFactory] context '%s' id=%08x jar='%s' klass='%s'\n",
        domain, id, (resolved.empty() ? (jarPath?jarPath:"") : resolved.c_str()), klassName?klassName:"<none>");
    return id;
}

bool KotlinFactory::unregisterContext(const char* domain) {
    if (!domain || !domain[0]) return false;
    return unregisterContext(hashDomain(domain));
}

bool KotlinFactory::unregisterContext(uint32_t domainHash) {
    jclass toDelete = nullptr;
    std::string domainName;
    uint32_t ctxId = 0;
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it = ctxs_.find(domainHash);
        if (it == ctxs_.end()) return false;
        toDelete = it->second.klass;
        domainName = it->second.domain;
        ctxId = it->second.id;
        ctxs_.erase(it);
    }
    if (toDelete) {
        JvmScheduler::instance().submitAndWait(JvmTask{[toDelete](JNIEnv* env){
            env->DeleteGlobalRef(toDelete);
        }});
    }
    if (ctxId) JvmFactory::instance().destroyContext(ctxId);
    SchemaFactory::instance().invalidateDomain(domainName.c_str());
    std::fprintf(stderr, "[KotlinFactory] unregistered '%s' %08x\n", domainName.c_str(), domainHash);
    return true;
}

uint32_t KotlinFactory::findContext(const char* domain) const {
    if (!domain || !domain[0]) return 0;
    uint32_t h = hashDomain(domain);
    std::lock_guard<std::mutex> lk(m_);
    auto it = ctxs_.find(h);
    return it == ctxs_.end() ? 0 : it->second.id;
}

bool KotlinFactory::hasContext(const char* domain) const {
    return findContext(domain) != 0;
}

void KotlinFactory::submit(const char* domain, std::function<void(JNIEnv*, jclass)> task) {
    if (!domain) return;
    submit(hashDomain(domain), std::move(task));
}

void KotlinFactory::submit(uint32_t domainHash, std::function<void(JNIEnv*, jclass)> task) {
    jclass klass = nullptr;
    uint32_t ctxId = 0;
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it = ctxs_.find(domainHash);
        if (it == ctxs_.end()) return;
        klass = it->second.klass;
        ctxId = it->second.id;
    }
    JvmFactory::instance().submit(ctxId, [klass, task=std::move(task)](JNIEnv* env){
        task(env, klass);
        if (env->ExceptionCheck()) JvmHost::hasException(env);
    });
}

bool KotlinFactory::callStaticVoid(const char* domain, const char* method, const char* sig) {
    if (!domain || !method || !sig) return false;
    uint32_t h = hashDomain(domain);
    jclass klass = nullptr;
    uint32_t ctxId = 0;
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it = ctxs_.find(h);
        if (it == ctxs_.end() || !it->second.klass) return false;
        klass = it->second.klass;
        ctxId = it->second.id;
        (void)ctxId;
    }
    std::atomic<bool> done{false};
    std::atomic<bool> ok{false};
    JvmFactory::instance().submit(ctxId, [klass, method, sig, &done, &ok](JNIEnv* env){
        jmethodID mid = env->GetStaticMethodID(klass, method, sig);
        if (!mid) { JvmHost::hasException(env); done.store(true); return; }
        env->CallStaticVoidMethod(klass, mid);
        if (env->ExceptionCheck()) JvmHost::hasException(env);
        else ok.store(true);
        done.store(true);
    });
    
    for (int i = 0; i < 200 && !done.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return ok.load();
}
