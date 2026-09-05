#pragma once
/// @file JVM.h
/// @brief Java (Kotlin/JVM) domain manager for modules — header-only ECS-style wrapper.
/// @details Mirrors `CLR.h` API shape and `ECS.h` ownership/attach pattern. Owns
///          one JVM domain (URLClassLoader + per-domain SchemaInstance).
/// @see ECS.h for the canonical pattern. @see CLR.h for CLR mirror.
/// @code
///   JVM jvm("MyJvm", "mod://my.jar");
///   jvm.define("Vec3").f32("x").f32("y").f32("z").commit();
///   jvm.registerNative("com/pkg/Bridge", "doWork", "(I)I", &doWork);
///   jvm.callStatic("com/pkg/Main", "main", "([Ljava/lang/String;)V");
/// @endcode

#include "FractalSDK.h"
#include "IKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <SDK/archetyped/jvm.h>
#include <SDK/archetyped/schema/SchemaInstancePayload.h>
#include <SDK/archetyped/schema/SchemaBlock.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>
#include <cstring>
#include <atomic>
#include <span>
#include <unordered_map>

/**
 * @class JVM
 * @brief One Java/Kotlin domain — single URLClassLoader, single SchemaInstance, single lifecycle.
 *
 * Constructors mirror `CLR`:
 *   - JVM(domain, jarPath)         — create new domain; destructor unregisters.
 *   - JVM(domain)                  — attach by name; no FURCMD, owned_=false.
 *   - JVM(hash)                    — attach by 32-bit hash.
 *   - JVM(name, existingId)        — attach with explicit id.
 */
class JVM {
public:
    /// @brief Create a new JVM domain (URLClassLoader + per-domain SchemaInstance).
    /// @param domain  Human-readable domain id, e.g. "MyJvm". Hashed with FNV-1a.
    /// @param jarPath VFS path (mod://...) or absolute path to a .jar.
    /// @note Blocks on a Ticket fence until the JVM host finishes CreateContext.
    JVM(std::string domain, std::string jarPath)
        : JVMName(domain), JVMJar(jarPath), owned_(true) {
        JVMId = fnv1aHash(domain);
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_jvm_create_ctx ctx{};
        std::snprintf(ctx.name, sizeof(ctx.name), "%s", domain.c_str());
        std::snprintf(ctx.jarPath, sizeof(ctx.jarPath), "%s", jarPath.c_str());
        ctx.outContextId = 0;
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_jvm_createContextHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload     = &ctx;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
        valid_ = true;
        if (ctx.outContextId) JVMId = ctx.outContextId;
    }

    /// @brief Attach to an existing domain by name (no FURCMD, owned_=false).
    explicit JVM(std::string domain)
        : JVMName(domain), JVMId(fnv1aHash(domain)), valid_(true), owned_(false) {}

    /// @brief Attach to an existing domain by 32-bit FNV-1a hash.
    explicit JVM(uint32_t domainHash)
        : JVMId(domainHash), JVMName(std::to_string(domainHash)), valid_(true), owned_(false) {}

    /// @brief Attach with explicit human-readable name and id.
    JVM(std::string domain, uint32_t existingId)
        : JVMName(domain), JVMId(existingId), valid_(true), owned_(false) {}

    JVM(const JVM&) = delete;
    JVM& operator=(const JVM&) = delete;
    JVM(JVM&&) = default;
    JVM& operator=(JVM&&) = default;

    /// @brief Destructor. Calls `destroy` only if `owned_`.
    ~JVM() {
        if (!valid_ || !owned_) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_jvm_destroy_ctx ctx{}; ctx.contextId = JVMId;
        FURCMDPacket packet{}; packet.methodHash=FractalSDK::arche_jvm_destroyContextHash;
        packet.payloadSize=sizeof(ctx); packet.payload=&ctx;
        sdk->sendPacket(packet);
    }

    /// @brief Register a struct into the JVM domain's SchemaInstance.
    /// @param name Struct name.
    /// @param fields vector of (SCHEMA_*, "fieldName") pairs, max 16.
    void registerStruct(const char* name, const std::vector<std::pair<uint32_t,std::string>>& fields) {
        if (!valid_||!name||fields.size()>16) return;
        auto* sdk=FractalSDK::SDK::Get(); if(!sdk) return;
        SchemaRegisterSchemaPayload p{}; std::snprintf(p.domain,sizeof(p.domain),"%s",JVMName.c_str());
        std::snprintf(p.name,sizeof(p.name),"%s",name); p.fieldCount=(uint32_t)fields.size();
        for(size_t i=0;i<fields.size();++i){ p.fields[i].typeId=fields[i].first; std::snprintf(p.fields[i].name,sizeof(p.fields[i].name),"%s",fields[i].second.c_str()); }
        Ticket* t=sdk->allocateTicket(); FURCMDPacket pkt{}; pkt.methodHash=FractalSDK::arche_jvm_registerDomainHash;
        pkt.payloadSize=sizeof(p); pkt.payload=&p; pkt.fence=(uint64_t*)&t->fence; sdk->sendPacket(pkt); while(!t->isReady()){}
    }
    /// @brief Init-list overload: `(SCHEMA_F32, "x")` literals.
    void registerStruct(const char* name, std::initializer_list<std::pair<uint32_t,const char*>> fields) {
        std::vector<std::pair<uint32_t,std::string>> v;
        for (auto& kv : fields) v.emplace_back(kv.first, std::string(kv.second?kv.second:""));
        registerStruct(name, v);
    }

    /// @brief Register a JNI native function so JVM can call back into C++.
    /// @param className  Fully-qualified Java class name, e.g. "com.pkg.Bridge".
    /// @param methodName Method name, e.g. "doWork".
    /// @param sig        JNI signature, e.g. "(I)I".
    /// @param fn         Function pointer of type `arche_jvm_native_fn` (JNIEnv*, jobject, jargs...).
    void registerNative(const char* className, const char* methodName, const char* sig, FractalSDK::arche_jvm_native_fn fn) {
        if(!valid_||!className||!methodName||!sig||!fn) return;
        auto* sdk=FractalSDK::SDK::Get(); if(!sdk) return;
        FractalSDK::arche_jvm_register_native_ctx ctx{}; ctx.contextId=JVMId;
        std::snprintf(ctx.className,sizeof(ctx.className),"%s",className);
        std::snprintf(ctx.methodName,sizeof(ctx.methodName),"%s",methodName);
        std::snprintf(ctx.signature,sizeof(ctx.signature),"%s",sig); ctx.fnPtr=fn;
        Ticket* t=sdk->allocateTicket(); FURCMDPacket p{}; p.methodHash=FractalSDK::arche_jvm_registerNativeHash;
        p.payloadSize=sizeof(ctx); p.payload=&ctx; p.fence=(uint64_t*)&t->fence; sdk->sendPacket(p); while(!t->isReady()){}
    }

    /// @brief Compose this domain with another JVM domain: merges types + adds ALC-style import.
    void compose(const JVM& other){ compose(other.JVMName.c_str()); }
    /// @brief Compose with an import domain by name.
    void compose(const char* importDomain){
        if(!valid_||!importDomain) return; auto* sdk=FractalSDK::SDK::Get(); if(!sdk) return;
        FractalSDK::arche_jvm_compose_ctx ctx{}; ctx.targetId=JVMId; ctx.importId=fnv1aHash(importDomain);
        Ticket* t=sdk->allocateTicket(); FURCMDPacket p{}; p.methodHash=FractalSDK::arche_jvm_composeHash;
        p.payloadSize=sizeof(ctx); p.payload=&ctx; p.fence=(uint64_t*)&t->fence; sdk->sendPacket(p); while(!t->isReady()){}
    }

    /// @brief Fire-and-forget static call into a JVM class.
    /// @param className  Fully-qualified Java class, e.g. "com.pkg.Main".
    /// @param methodName Static method name, e.g. "main".
    /// @param sig        JNI signature, e.g. "([Ljava/lang/String;)V" (may be nullptr).
    void callStatic(const char* className,const char* methodName,const char* sig){
        if(!valid_||!className||!methodName) return; auto* sdk=FractalSDK::SDK::Get(); if(!sdk) return;
        FractalSDK::arche_jvm_call_static_ctx ctx{}; ctx.contextId=JVMId;
        std::snprintf(ctx.className,sizeof(ctx.className),"%s",className);
        std::snprintf(ctx.methodName,sizeof(ctx.methodName),"%s",methodName);
        if(sig) std::snprintf(ctx.signature,sizeof(ctx.signature),"%s",sig);
        FURCMDPacket p{}; p.methodHash=FractalSDK::arche_jvm_callStaticHash;
        p.payloadSize=sizeof(ctx); p.payload=&ctx; sdk->sendPacket(p);
    }
    /// @brief Call only if instance is valid and owned.
    bool callIf(const char* className, const char* methodName, const char* sig=nullptr) {
        if (!valid_ || !owned_) return false;
        callStatic(className, methodName, sig);
        return true;
    }

    /// @brief Explicit unregister + invalidate.
    void destroy(){
        if(!valid_) return; auto* sdk=FractalSDK::SDK::Get(); if(!sdk) return;
        FractalSDK::arche_jvm_destroy_ctx ctx{}; ctx.contextId=JVMId;
        Ticket* t=sdk->allocateTicket(); FURCMDPacket p{}; p.methodHash=FractalSDK::arche_jvm_destroyContextHash;
        p.payloadSize=sizeof(ctx); p.payload=&ctx; p.fence=(uint64_t*)&t->fence; sdk->sendPacket(p); while(!t->isReady()){} valid_=false;
    }

    /// @brief Fluent struct builder. Use via `define("Name")`, then chain field sugar and `commit()`.
    class StructBuilder {
    public:
        StructBuilder(JVM& o, std::string n) : owner_(o), name_(std::move(n)) {}
        /// @brief Raw field: SCHEMA_* + name.
        StructBuilder& field(uint32_t id,const char* f){ if(fields_.size()<16) fields_.emplace_back(id,std::string(f?f:"")); return *this; }
        StructBuilder& field(uint32_t id,std::string f){ if(fields_.size()<16) fields_.emplace_back(id,std::move(f)); return *this; }
        // numeric sugar
        StructBuilder& boolean(const char* n){ return field(SCHEMA_BOOL,   n); }  ///< JVM `boolean`/`Boolean`
        StructBuilder& u8 (const char* n)    { return field(SCHEMA_U8,     n); }  ///< JVM `byte`
        StructBuilder& u16(const char* n)    { return field(SCHEMA_U16,    n); }  ///< JVM `char`/`short`
        StructBuilder& u32(const char* n)    { return field(SCHEMA_U32,    n); }  ///< JVM `int`/`float`
        StructBuilder& u64(const char* n)    { return field(SCHEMA_U64,    n); }  ///< JVM `long`
        StructBuilder& i8 (const char* n)    { return field(SCHEMA_I8,     n); }  ///< JVM `byte` (signed)
        StructBuilder& i16(const char* n)    { return field(SCHEMA_I16,    n); }  ///< JVM `short`
        StructBuilder& i32(const char* n)    { return field(SCHEMA_I32,    n); }  ///< JVM `int`
        StructBuilder& i64(const char* n)    { return field(SCHEMA_I64,    n); }  ///< JVM `long`
        StructBuilder& f32(const char* n)    { return field(SCHEMA_F32,    n); }  ///< JVM `float`
        StructBuilder& f64(const char* n)    { return field(SCHEMA_F64,    n); }  ///< JVM `double`
        StructBuilder& ptr(const char* n)    { return field(SCHEMA_PTR,    n); }  ///< JVM `long` (raw pointer)
        StructBuilder& handle(const char* n) { return field(SCHEMA_HANDLE, n); }  ///< JVM `long` (jobject handle)
        StructBuilder& strct(const char* n)  { return field(SCHEMA_STRUCT, n); }  ///< nested struct/class
        /// @brief Fixed-size array: count elements of typeId (codegen turns into `T[N]`).
        StructBuilder& array(uint32_t typeId, const char* n, uint32_t count) {
            if (fields_.size() < 16) fields_.emplace_back(typeId | (count << 16), std::string(n?n:""));
            return *this;
        }
        /// @brief Send accumulated fields to the per-domain SchemaInstance.
        void commit(){ owner_.registerStruct(name_.c_str(), fields_); }
        /// @brief Alias of `commit()` for readability at end of chain.
        bool end(){ commit(); return true; }
    private: JVM& owner_; std::string name_; std::vector<std::pair<uint32_t,std::string>> fields_;
    };
    /// @brief Start a fluent struct definition.
    StructBuilder define(const char* n){ return StructBuilder(*this, n?n:""); }
    StructBuilder struct_(const char* n){ return define(n); }
    StructBuilder type(const char* n){ return define(n); }

    /// @brief Get the human-readable domain name.
    std::string getDomainName() const { return JVMName; }
    /// @brief Get the 32-bit FNV-1a domain id.
    uint32_t getDomainHashId() const { return JVMId; }
    /// @brief True if the domain was successfully created/attached.
    bool valid() const { return valid_; }
    /// @brief True if this instance is the original owner.
    bool owns() const  { return owned_; }
private:
    uint32_t JVMId=0;
    std::string JVMName;
    std::string JVMJar;
    bool valid_=false;
    bool owned_=false;
    std::unordered_map<uint32_t, void*> directCache_; ///< reserved for future JNI direct cache
};

namespace FractalSDK { namespace JVMNS { using Domain = ::JVM; } }
namespace FractalSDK { using JVMDomain = ::JVM; }
