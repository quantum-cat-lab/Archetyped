#pragma once
/// @file CLR.h
/// @brief C# / .NET CLR domain manager for modules — header-only ECS-style wrapper.
/// @details Owns one CLR domain (AssemblyLoadContext + per-domain SchemaInstance).
///          Mirrors `JVM.h` API shape and `ECS.h` ownership/attach pattern.
/// @see ECS.h for the canonical pattern. @see JVM.h for JVM mirror.
/// @code
///   CLR clr("HelloWorld", "mod://Arche.HelloWorld.dll", "Arche.HelloWorld.Entry");
///   clr.define("Vec3").f32("x").f32("y").f32("z").commit();
///   clr.installDirect("MyNative", SCHEMA_I32, {SCHEMA_I32}, (void*)&myFn);
///   using TickFn = void(*)();
///   auto tick = clr.bind<TickFn>("Arche.HelloWorld.Entry", "Tick");
///   tick(); // 2-3ns direct call, cached by FNV-1a hash
/// @endcode

#include "FractalSDK.h"
#include "IKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <SDK/archetyped/csharp.h>
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
 * @class CLR
 * @brief One C# (.NET) domain — single ALC, single SchemaInstance, single lifecycle.
 *
 * Constructors:
 *   - CLR(domain, dllPath, entryType) — creates a new domain; destructor unregisters.
 *   - CLR(domain) — attach by name; no FURCMD, owned_=false.
 *   - CLR(hash) — attach by 32-bit hash.
 *   - CLR(name, existingId) — attach with explicit id.
 *
 * Threading: each method is one-shot; reuse the same `CLR` instance from one thread.
 */
class CLR {
public:
    /// @brief Create a new C# domain (CreateALC + Load + GetFunctionPointer + SchemaInstance).
    /// @param domain   Human-readable domain id, e.g. "HelloWorld". Hashed with FNV-1a.
    /// @param dllPath  VFS path (mod://...) or absolute path to the .dll.
    /// @param entryType Fully-qualified C# type implementing the static entry point, e.g. "Arche.HelloWorld.Entry".
    /// @note Blocks on a Ticket fence until the CLR host finishes CreateALC.
    CLR(std::string domain, std::string dllPath, std::string entryType)
        : CLRDomainName(domain), CLRDllPath(dllPath), CLREntryType(entryType), owned_(true) {
        CLRDomainId = fnv1aHash(domain);
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_csharp_register_ctx ctx{};
        std::snprintf(ctx.domain, sizeof(ctx.domain), "%s", domain.c_str());
        std::snprintf(ctx.dllPath, sizeof(ctx.dllPath), "%s", dllPath.c_str());
        std::snprintf(ctx.entryType, sizeof(ctx.entryType), "%s", entryType.c_str());
        ctx.outContextId = 0;
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_csharp_registerContextHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload     = &ctx;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
        valid_ = true;
        if (ctx.outContextId) CLRDomainId = ctx.outContextId;
    }

    /// @brief Attach to an existing domain by name (no FURCMD, owned_=false).
    explicit CLR(std::string domain)
        : CLRDomainName(domain), CLRDomainId(fnv1aHash(domain)), valid_(true), owned_(false) {}

    /// @brief Attach to an existing domain by 32-bit FNV-1a hash.
    explicit CLR(uint32_t domainHash)
        : CLRDomainId(domainHash), CLRDomainName(std::to_string(domainHash)), valid_(true), owned_(false) {}

    /// @brief Attach with explicit human-readable name and id (cosmetic).
    CLR(std::string domain, uint32_t existingId)
        : CLRDomainName(domain), CLRDomainId(existingId), valid_(true), owned_(false) {}

    CLR(const CLR&) = delete;
    CLR& operator=(const CLR&) = delete;
    CLR(CLR&&) = default;
    CLR& operator=(CLR&&) = default;

    /// @brief Destructor. Calls `unregister` only if `owned_` (this instance created the domain).
    ~CLR() {
        if (!valid_ || !owned_) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_csharp_unregister_ctx ctx{};
        std::snprintf(ctx.domain, sizeof(ctx.domain), "%s", CLRDomainName.c_str());
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_csharp_unregisterContextHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload     = &ctx;
        sdk->sendPacket(packet);
    }

    /// @brief Register a C struct type into the domain's SchemaInstance.
    /// @param name Struct name, e.g. "Vec3".
    /// @param fields vector of (SCHEMA_*, "fieldName") pairs, max 16.
    /// @note C# side must declare `[StructLayout(LayoutKind.Sequential, Pack=4)] struct Vec3 { public float x,y,z; }`.
    void registerStruct(const char* name, const std::vector<std::pair<uint32_t, std::string>>& fields) {
        if (!valid_ || !name) return;
        if (fields.size() > 16) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        SchemaRegisterSchemaPayload p{};
        std::snprintf(p.domain, sizeof(p.domain), "%s", CLRDomainName.c_str());
        std::snprintf(p.name, sizeof(p.name), "%s", name);
        p.fieldCount = static_cast<uint32_t>(fields.size());
        for (size_t i = 0; i < fields.size(); ++i) {
            p.fields[i].typeId = fields[i].first;
            std::snprintf(p.fields[i].name, sizeof(p.fields[i].name), "%s", fields[i].second.c_str());
        }
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_clr_registerStructHash;
        packet.payloadSize = sizeof(p);
        packet.payload     = &p;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
    }
    /// @brief Convenience overload: init-list of (type, "name") literals.
    void registerStruct(const char* name, std::initializer_list<std::pair<uint32_t, const char*>> fields) {
        if (!valid_ || !name) return;
        if (fields.size() > 16) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        SchemaRegisterSchemaPayload p{};
        std::snprintf(p.domain, sizeof(p.domain), "%s", CLRDomainName.c_str());
        std::snprintf(p.name, sizeof(p.name), "%s", name);
        p.fieldCount = static_cast<uint32_t>(fields.size());
        size_t i = 0;
        for (auto& kv : fields) {
            p.fields[i].typeId = kv.first;
            std::snprintf(p.fields[i].name, sizeof(p.fields[i].name), "%s", kv.second);
            ++i;
        }
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_clr_registerStructHash;
        packet.payloadSize = sizeof(p);
        packet.payload     = &p;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
    }

    /// @brief Compose this domain with another: merges types + registers ALC import (variant A).
    /// @param other Source domain whose types this domain should see.
    void compose(const CLR& other) { compose(other.CLRDomainName.c_str()); }
    /// @brief Compose with an import domain by name.
    /// @param importDomain Name of the source domain.
    void compose(const char* importDomain) {
        if (!valid_ || !importDomain) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_clr_compose_ctx ctx{};
        ctx.targetId = CLRDomainId;
        ctx.importId = fnv1aHash(importDomain);
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_clr_composeHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload     = &ctx;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
    }

    /// @brief Fire-and-forget static method invocation (no fence; ~5-15us via FURCMD queue).
    /// @note Use `bind<Fn>` instead for hot paths.
    void callStatic(const char* typeName, const char* methodName) {
        if (!valid_ || !typeName || !methodName) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_csharp_call_static_ctx ctx{};
        std::snprintf(ctx.domain, sizeof(ctx.domain), "%s", CLRDomainName.c_str());
        std::snprintf(ctx.typeName, sizeof(ctx.typeName), "%s", typeName);
        std::snprintf(ctx.methodName, sizeof(ctx.methodName), "%s", methodName);
        ctx.ok = 0;
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_csharp_callStaticHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload     = &ctx;
        sdk->sendPacket(packet);
    }
    /// @brief Alias of `callStatic`.
    void call(const char* typeName, const char* methodName) { callStatic(typeName, methodName); }

    /// @brief Convenience: call only if instance is valid and owned (init-style call).
    /// @return true if a call was actually issued.
    bool callIf(const char* typeName, const char* methodName) {
        if (!valid_ || !owned_) return false;
        callStatic(typeName, methodName);
        return true;
    }

    // ---- Direct 0-overhead bridge ----
    /// @brief Expose a native C/C++ function to C# as `[UnmanagedCallersOnly]` target.
    /// @param methodName Name under which C# sees the function (C# passes it as DllImport entry).
    /// @param retType    SCHEMA_* return type (FNV-1a for nested struct).
    /// @param argTypes   span<SCHEMA_*> of argument types.
    /// @param fnPtr      Raw function pointer, must be `void*`-compatible.
    void installDirect(const char* methodName, uint32_t retType, std::span<const uint32_t> argTypes, void* fnPtr) {
        if (!methodName || !fnPtr) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_clr_installDirect_ctx p{};
        std::snprintf(p.methodName, sizeof(p.methodName), "%s", methodName);
        p.methodHash = fnv1aHash(methodName);
        p.fnPtr = fnPtr;
        p.retType = retType;
        p.argCount = (uint32_t)std::min<size_t>(argTypes.size(), 8);
        for (uint32_t i=0;i<p.argCount;++i) p.argTypes[i]=argTypes[i];
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_clr_installDirectHash;
        packet.payloadSize = sizeof(p);
        packet.payload     = &p;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
    }
    /// @brief Install with a pre-computed 32-bit hash (no string copy).
    void installDirect(uint32_t methodHash, void* fnPtr) {
        if (!methodHash || !fnPtr) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_clr_installDirect_ctx p{};
        p.methodHash = methodHash;
        p.fnPtr = fnPtr;
        p.retType = SCHEMA_VOID;
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_clr_installDirectHash;
        packet.payloadSize = sizeof(p);
        packet.payload     = &p;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
    }
    /// @brief Install with `SCHEMA_VOID` return and no args — sugar.
    void installDirect(const char* methodName, void* fnPtr) { installDirect(methodName, SCHEMA_VOID, {}, fnPtr); }

    /// @brief Uninstall a previously installed C->C# entry.
    /// @note Currently a no-op stub; reserved for hot-reload support.
    void uninstallDirect(const char* methodName) {
        if (!methodName) return;
        // TODO: send `archetyped:clr:uninstallDirect` once added to BridgeRegistry
    }

    // ---- C++ -> C# direct cached ----
    /// @brief Get a cached C# function pointer for `static void M()` style entries.
    /// @tparam Fn Function pointer type, e.g. `void(*)()`.
    /// @param typeName C# type with the static method.
    /// @param methodName C# method name.
    /// @return typed function pointer, or nullptr on failure.
    /// @note First call hits `Bootstrap.GetFunctionPointer` via `outputBuffer`; subsequent calls are 2-3ns hashmap lookup.
    template<typename Fn>
    Fn bind(const char* typeName, const char* methodName) {
        if (!valid_ || !typeName || !methodName) return nullptr;
        uint32_t h = fnv1aHash(typeName) ^ (fnv1aHash(methodName) * 16777619u) ^ CLRDomainId;
        auto it = directCache_.find(h);
        if (it != directCache_.end()) return reinterpret_cast<Fn>(it->second);
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return nullptr;
        struct In { char domain[64]; char typeName[256]; char methodName[64]; } in{};
        std::snprintf(in.domain, sizeof(in.domain), "%s", CLRDomainName.c_str());
        std::snprintf(in.typeName, sizeof(in.typeName), "%s", typeName);
        std::snprintf(in.methodName, sizeof(in.methodName), "%s", methodName);
        struct Out { void* func = nullptr; int32_t ok = 0; } out{};
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_csharp_getFuncHash;
        packet.payloadSize = sizeof(in);
        packet.payload     = &in;
        packet.outputBuffer = &out;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
        if (out.ok && out.func) {
            directCache_[h] = out.func;
            return reinterpret_cast<Fn>(out.func);
        }
        return nullptr;
    }
    /// @brief Out-parameter overload for `bind`.
    template<typename Fn>
    bool bind(const char* typeName, const char* methodName, Fn* out) {
        if (!out) return false;
        *out = bind<Fn>(typeName, methodName);
        return *out != nullptr;
    }
    /// @brief Checked variant: cheap SCHEMA range validation before caching.
    /// @param retType expected SCHEMA_* return type, or SCHEMA_VOID.
    /// @param argTypes expected SCHEMA_* arg types.
    /// @return typed function pointer, or nullptr on validation failure.
    template<typename Fn>
    Fn bindChecked(const char* typeName, const char* methodName, uint32_t retType, std::span<const uint32_t> argTypes) {
        if (!valid_ || !typeName || !methodName) return nullptr;
        auto validSchema = [](uint32_t t){ return t <= SCHEMA_STRUCT || t == SCHEMA_VOID; };
        if (!validSchema(retType)) return nullptr;
        for (auto a : argTypes) if (!validSchema(a)) return nullptr;
        return bind<Fn>(typeName, methodName);
    }
    template<typename Fn>
    Fn bindChecked(const char* typeName, const char* methodName, uint32_t retType, std::initializer_list<uint32_t> args) {
        return bindChecked<Fn>(typeName, methodName, retType, std::span<const uint32_t>(args.begin(), args.size()));
    }
    /// @brief Drop all cached C# function pointers. Call after UnloadALC or on hot-reload.
    void clearDirectCache() { directCache_.clear(); }

    /// @brief Explicit unregister + invalidate this domain.
    /// @note Idempotent; safe to call from destructors of long-lived modules.
    void destroy() {
        if (!valid_) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;
        FractalSDK::arche_csharp_unregister_ctx ctx{};
        std::snprintf(ctx.domain, sizeof(ctx.domain), "%s", CLRDomainName.c_str());
        Ticket* ticket = sdk->allocateTicket();
        FURCMDPacket packet{};
        packet.methodHash  = FractalSDK::arche_csharp_unregisterContextHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload     = &ctx;
        packet.fence       = (uint64_t*)&ticket->fence;
        sdk->sendPacket(packet);
        while (!ticket->isReady()) {}
        valid_ = false;
        directCache_.clear();
    }

    /// @brief Fluent struct-builder. Use via `define("Name")`, then chain `.f32().u32().commit()`.
    class StructBuilder {
    public:
        StructBuilder(CLR& owner, std::string name) : owner_(owner), name_(std::move(name)) {}
        /// @brief Raw field: SCHEMA_* + name.
        StructBuilder& field(uint32_t typeId, const char* fname) {
            if (fields_.size() < 16) fields_.emplace_back(typeId, std::string(fname ? fname : ""));
            return *this;
        }
        StructBuilder& field(uint32_t typeId, std::string fname) {
            if (fields_.size() < 16) fields_.emplace_back(typeId, std::move(fname));
            return *this;
        }
        // ---- numeric sugar (single token per field) ----
        StructBuilder& boolean(const char* n)  { return field(SCHEMA_BOOL,   n); }  ///< C# `bool`
        StructBuilder& u8 (const char* n)      { return field(SCHEMA_U8,     n); }  ///< C# `byte`
        StructBuilder& u16(const char* n)      { return field(SCHEMA_U16,    n); }  ///< C# `ushort`
        StructBuilder& u32(const char* n)      { return field(SCHEMA_U32,    n); }  ///< C# `uint`
        StructBuilder& u64(const char* n)      { return field(SCHEMA_U64,    n); }  ///< C# `ulong`
        StructBuilder& i8 (const char* n)      { return field(SCHEMA_I8,     n); }  ///< C# `sbyte`
        StructBuilder& i16(const char* n)      { return field(SCHEMA_I16,    n); }  ///< C# `short`
        StructBuilder& i32(const char* n)      { return field(SCHEMA_I32,    n); }  ///< C# `int`
        StructBuilder& i64(const char* n)      { return field(SCHEMA_I64,    n); }  ///< C# `long`
        StructBuilder& f32(const char* n)      { return field(SCHEMA_F32,    n); }  ///< C# `float`
        StructBuilder& f64(const char* n)      { return field(SCHEMA_F64,    n); }  ///< C# `double`
        StructBuilder& str(const char* n)      { return field(SCHEMA_U8,     n); }  ///< C# `string` (treated as bytes via codegen)
        StructBuilder& ptr(const char* n)      { return field(SCHEMA_PTR,    n); }  ///< C# `IntPtr`
        StructBuilder& handle(const char* n)   { return field(SCHEMA_HANDLE, n); }  ///< C# `IntPtr`
        StructBuilder& strct(const char* n)    { return field(SCHEMA_STRUCT, n); }  ///< nested struct
        /// @brief Fixed-size array: count elements of typeId (codegen turns into `fixed` buffer).
        StructBuilder& array(uint32_t typeId, const char* n, uint32_t count) {
            // arrays are stored as a single field with the array count encoded into the
            // upper 16 bits of typeId (Codegen recognizes the high bits and emits `fixed T[N]`).
            if (fields_.size() < 16) fields_.emplace_back(typeId | (count << 16), std::string(n ? n : ""));
            return *this;
        }
        /// @brief Send the accumulated fields to the per-domain SchemaInstance.
        void commit() { owner_.registerStruct(name_.c_str(), fields_); }
        /// @brief Alias of `commit()` for readability at end of chain.
        bool end()    { commit(); return true; }
    private:
        CLR& owner_;
        std::string name_;
        std::vector<std::pair<uint32_t,std::string>> fields_;
    };
    /// @brief Start a fluent struct definition. Synonym: `struct_`, `type`.
    StructBuilder define(const char* name) { return StructBuilder(*this, name ? name : ""); }
    StructBuilder struct_(const char* name) { return define(name); }
    StructBuilder type(const char* name)    { return define(name); }

    /// @brief Get the human-readable domain name.
    std::string getDomainName() const { return CLRDomainName; }
    /// @brief Get the 32-bit FNV-1a domain id (assigned by CSharpFactory on create, or hash-of-name for attach).
    uint32_t getDomainHashId() const { return CLRDomainId; }
    /// @brief True if the domain was successfully created/attached.
    bool valid() const { return valid_; }
    /// @brief True if this instance is the original owner (its destructor will unregister).
    bool owns() const  { return owned_; }
private:
    uint32_t CLRDomainId = 0;
    std::string CLRDomainName;
    std::string CLRDllPath;
    std::string CLREntryType;
    bool valid_ = false;
    bool owned_ = false;
    std::unordered_map<uint32_t, void*> directCache_;
};

/// @brief Alias inside the `FractalSDK` namespace — match `FractalSDK::CLR::Domain` usage.
namespace FractalSDK { namespace CLR { using Domain = ::CLR; } }
/// @brief Plain alias for `CLR` to keep older code compiling.
namespace FractalSDK { using CLRDomain = ::CLR; }
