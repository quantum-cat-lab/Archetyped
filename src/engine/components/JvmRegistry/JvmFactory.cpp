#include "JvmFactory.h"
#include "../SchemaRegistry/SchemaFactory.h"
#include "../SchemaRegistry/SchemaInstance.h"
#include <SDK/archetyped/schema/SchemaRegistryPayload.h>

#include "../../core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstdio>
#include <iostream>

namespace {
    constexpr uint32_t registerJvmDomainHash = fnv1aHashConst("archetyped:jvm:registerInstance");
    constexpr uint32_t createContextHash    = fnv1aHashConst("archetyped:jvm:createInstance");
    constexpr uint32_t destroyContextHash   = fnv1aHashConst("archetyped:jvm:destroyInstance");
    constexpr uint32_t composeHash          = fnv1aHashConst("archetyped:jvm:compose");
    constexpr uint32_t registerNativeHash   = fnv1aHashConst("archetyped:jvm:registerNative");
    constexpr uint32_t registerSchemaHash   = fnv1aHashConst("archetyped:schema:registerSchema");
    constexpr uint32_t dumpSchemaHash       = fnv1aHashConst("archetyped:schema:dumpSchema");

    uint32_t ctxHash(const char* name) { return fnv1aHash(name); }
}

JvmFactory& JvmFactory::instance() {
    static JvmFactory inst;
    return inst;
}

JvmScheduler& JvmScheduler::instance() {
    static JvmScheduler inst;
    return inst;
}

void JvmFactory::registerJvmDomain(FURCMDPacket& pkt) {
    (void)pkt;
    FractalKernel::instance().registerCMDMethod(createContextHash, createContextCMD);
    FractalKernel::instance().registerCMDMethod(destroyContextHash, destroyContextCMD);
    FractalKernel::instance().registerCMDMethod(composeHash, composeCMD);
    FractalKernel::instance().registerCMDMethod(registerNativeHash, registerNativeCMD);
    FractalKernel::instance().registerCMDMethod(registerSchemaHash, registerSchemaCMD);
    FractalKernel::instance().registerCMDMethod(dumpSchemaHash, dumpSchemaCMD);
    std::fprintf(stderr, "[JvmFactory] domain registered\n");
}

void JvmFactory::createContextCMD(FURCMDPacket& pkt) {
    auto* c = static_cast<jvmCreateInstanceCtx*>(pkt.payload);
    c->outContextId = instance().createContext(c->name, c->jarPath);
}

void JvmFactory::destroyContextCMD(FURCMDPacket& pkt) {
    auto* c = static_cast<jvmDestroyInstanceCtx*>(pkt.payload);
    instance().destroyContext(c->contextId);
}

void JvmFactory::composeCMD(FURCMDPacket& pkt) {
    auto* c = static_cast<jvmComposeInstanceCtx*>(pkt.payload);
    instance().compose(c->targetId, c->importId);
}

void JvmFactory::registerNativeCMD(FURCMDPacket& pkt) {
    auto* c = static_cast<jvmRegisterNativeInstanceCtx*>(pkt.payload);
    instance().registerNative(c->contextId, c->className, c->methodName, c->signature, c->fnPtr);
}

void JvmFactory::registerSchemaCMD(FURCMDPacket& pkt) {
    auto* c = static_cast<SchemaRegisterSchemaPayload*>(pkt.payload);
    if (!c || !pkt.payloadSize) return;
    std::vector<std::pair<uint32_t, std::string>> fields;
    fields.reserve(c->fieldCount);
    for (uint32_t i = 0; i < c->fieldCount; ++i) {
        fields.emplace_back(c->fields[i].typeId, c->fields[i].name);
    }
    // domain name in payload may be empty -> use JVM context name when available
    const char* domain = c->domain[0] ? c->domain : "jvm";
    uint32_t ctxHash = fnv1aHash(domain);
    {
        auto& self = instance();
        std::lock_guard<std::mutex> lk(self.m_);
        auto it = self.ctxs_.find(ctxHash);
        if (it != self.ctxs_.end()) {
            if (!it->second.schemaReg) it->second.schemaReg = std::make_unique<SchemaInstance>();
            it->second.schemaReg->registerSchema(c->name, fields);
        } else {
            // fallback to global mgr for compatibility bc global SchemaInstance can handle it idk
            SchemaFactory::instance().registerSchema(domain, c->name, fields);
            std::cout << "[JvmFactory][Schema] fallback to the global Mgr" << std::endl;
        }
    }
    std::fprintf(stderr, "[JvmFactory][Schema] domain='%s' schema='%s' fields=%u\n", domain, c->name, c->fieldCount);
}

void JvmFactory::dumpSchemaCMD(FURCMDPacket& pkt) {
    auto* p = static_cast<SchemaDumpSchemaInstancePayload*>(pkt.payload);
    if (!p || !p->outputBuffer) return;
    auto buf = SchemaFactory::instance().dumpSchema(p->domain[0] ? p->domain : "jvm");
    p->outSize = 0;
    if (!buf.empty() && buf.size() <= p->bufferSize) {
        std::memcpy(p->outputBuffer, buf.data(), buf.size());
        p->outSize = static_cast<uint32_t>(buf.size());
    }
    if (pkt.fence) *pkt.fence = 1;
    if (p->fence) *p->fence = 1;
}

jobject JvmFactory::makeUrlClassLoader(JNIEnv* env, const char* jarPath) {
    jclass urlCls = env->FindClass("java/net/URL");
    jclass fileCls = env->FindClass("java/io/File");
    jclass loaderCls = env->FindClass("java/net/URLClassLoader");
    if (!urlCls || !fileCls || !loaderCls) return nullptr;

    jmethodID fileCtor = env->GetMethodID(fileCls, "<init>", "(Ljava/lang/String;)V");
    jmethodID toURL = env->GetMethodID(fileCls, "toURI", "()Ljava/net/URI;");
    jmethodID loaderCtor = env->GetMethodID(loaderCls, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V");
    jclass uriCls = env->FindClass("java/net/URI");
    jmethodID uriToURL = env->GetMethodID(uriCls, "toURL", "()Ljava/net/URL;");

    jobject file = env->NewObject(fileCls, fileCtor, env->NewStringUTF(jarPath));
    jobject uri = env->CallObjectMethod(file, toURL);
    jobject url = env->CallObjectMethod(uri, uriToURL);

    jobjectArray urls = env->NewObjectArray(1, urlCls, url);
    jclass clCls = env->FindClass("java/lang/ClassLoader");
    jobject parent = env->CallStaticObjectMethod(clCls,
        env->GetStaticMethodID(clCls, "getSystemClassLoader", "()Ljava/lang/ClassLoader;"));
    jobject loader = env->NewObject(loaderCls, loaderCtor, urls, parent);
    if (env->ExceptionCheck()) { JvmHost::hasException(env); return nullptr; }
    return loader;
}

uint32_t JvmFactory::createContext(const char* name, const char* jarPath) {
    uint32_t id = ctxHash(name);
    sched_.submitAndWait(JvmTask{[&](JNIEnv* env) {
        jobject loader = makeUrlClassLoader(env, jarPath);
        if (!loader) return;
        JvmInstance ctx{ id, name, env->NewGlobalRef(loader), {}, std::make_unique<SchemaInstance>() };
        if (id != kCoreCtx) ctx.imports.push_back(kCoreCtx);
        std::lock_guard<std::mutex> lk(m_);
        ctxs_[id] = std::move(ctx);
        std::fprintf(stderr, "[JvmFactory] context %s (id=%08x) created\n", name, id);
    }});
    return id;
}

void JvmFactory::registerSchema(uint32_t ctxId, const char* name,
                            const std::vector<std::pair<uint32_t, std::string>>& fields) {
    std::lock_guard<std::mutex> lk(m_);
    auto it = ctxs_.find(ctxId);
    if (it == ctxs_.end()) return;
    if (!it->second.schemaReg) it->second.schemaReg = std::make_unique<SchemaInstance>();
    it->second.schemaReg->registerSchema(name, fields);
}

void JvmFactory::destroyContext(uint32_t id) {
    sched_.submitAndWait(JvmTask{[&](JNIEnv* env) {
        std::lock_guard<std::mutex> lk(m_);
        auto it = ctxs_.find(id);
        if (it == ctxs_.end()) return;
        if (it->second.classLoader) env->DeleteGlobalRef(it->second.classLoader);
        ctxs_.erase(it);
        std::fprintf(stderr, "[JvmFactory] context %08x destroyed\n", id);
    }});
}

void JvmFactory::compose(uint32_t target, uint32_t importId) {
    std::lock_guard<std::mutex> lk(m_);
    auto it = ctxs_.find(target);
    if (it == ctxs_.end()) return;
    auto imp = ctxs_.find(importId);
    if (imp == ctxs_.end()) return;
    if (std::find(it->second.imports.begin(), it->second.imports.end(), importId) == it->second.imports.end())
        it->second.imports.push_back(importId);
    // merge SchemaInstance like ClrFactory
    if (imp->second.schemaReg && it->second.schemaReg) {
        it->second.schemaReg->mergeFrom(*imp->second.schemaReg);
    }
    std::fprintf(stderr, "[JvmFactory] compose %08x <- %08x\n", target, importId);
}

void JvmFactory::registerNative(uint32_t ctxId, const char* cls, const char* method,
                                 const char* sig, void* fnPtr) {
    sched_.submitAndWait(JvmTask{[&](JNIEnv* env) {
        jclass c = findClass(env, ctxId, cls);
        if (!c) return;
        JNINativeMethod nm{ const_cast<char*>(method), const_cast<char*>(sig), fnPtr };
        if (env->RegisterNatives(c, &nm, 1) != JNI_OK)
            JvmHost::hasException(env);
    }});
}

jclass JvmFactory::findClass(JNIEnv* env, uint32_t ctxId, const char* name) {
    std::lock_guard<std::mutex> lk(m_);
    auto it = ctxs_.find(ctxId);
    if (it == ctxs_.end()) return nullptr;

    jclass loaderCls = env->FindClass("java/net/URLClassLoader");
    jmethodID loadM = env->GetMethodID(loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    std::string dotted(name ? name : "");
    for (char &c : dotted) if (c == '/') c = '.';
    auto tryLoad = [&](jobject loader) -> jclass {
        if (!loader) return nullptr;
        jclass r = static_cast<jclass>(env->CallObjectMethod(
            loader, loadM, env->NewStringUTF(dotted.c_str())));
        if (r && !env->ExceptionCheck()) return r;
        env->ExceptionClear();
        r = env->FindClass(name);
        if (r && !env->ExceptionCheck()) return r;
        env->ExceptionClear();
        return nullptr;
    };

    if (jclass r = tryLoad(it->second.classLoader)) return r;
    for (uint32_t imp : it->second.imports)
        if (jclass r = tryLoad(ctxs_[imp].classLoader)) return r;
    return nullptr;
}

void JvmFactory::submit(uint32_t ctxId, std::function<void(JNIEnv*)> task) {
    sched_.submit(JvmTask{[ctxId, task](JNIEnv* env) { task(env); }});
}