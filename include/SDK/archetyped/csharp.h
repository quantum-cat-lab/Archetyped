#pragma once
/// @file csharp.h
/// @brief C# / .NET bridge SDK — C-compatible POD layer for FURCMD payloads.
/// @details Module never includes hostfxr/coreclr headers. All CLR types opaque.
///          Structures are POD with fixed layout so FURCMD can memcpy them as payload.
///          Mirrors `jvm.h` pattern: domain = hash string, dllPath via VFS `mod://`.
/// @see JVM.h for Java mirror. @see CLR.h for the high-level manager.
/// @code
///   // C++ side usage:
///   FractalSDK::arche_csharp_register_ctx ctx{};
///   std::snprintf(ctx.domain, 64, "HelloWorld");
///   std::snprintf(ctx.dllPath, 1024, "mod://Arche.HelloWorld.dll");
///   std::snprintf(ctx.entryType, 256, "Arche.HelloWorld.Entry");
///   packet.methodHash = FractalSDK::arche_csharp_registerContextHash;
///   packet.payload = &ctx;
/// @endcode

#include <SDK/archetyped/hash/hash.h>
#include <cstdint>

namespace FractalSDK {

/**
 * @struct arche_csharp_register_ctx
 * @brief Payload for `archetyped:csharp:registerContext` — creates a CLR domain + ALC.
 */
struct arche_csharp_register_ctx {
    char     domain[64];      ///< Domain id, e.g. "HelloWorld". Hashed FNV-1a.
    char     dllPath[1024];   ///< VFS `mod://Arche.Foo.dll` or absolute path.
    char     entryType[256];  ///< Fully-qualified C# entry, e.g. "Arche.Foo.Entry".
    uint32_t outContextId;    ///< OUT: assigned domain id (0 = failure).
};

/**
 * @struct arche_csharp_unregister_ctx
 * @brief Payload for `archetyped:csharp:unregisterContext` — unloads ALC + drops SchemaInstance.
 */
struct arche_csharp_unregister_ctx {
    char domain[64];          ///< Domain id to unregister.
};

/**
 * @struct arche_csharp_call_static_ctx
 * @brief Payload for `archetyped:csharp:callStatic` — fire-and-forget static call.
 */
struct arche_csharp_call_static_ctx {
    char    domain[64];       ///< Target domain id.
    char    typeName[256];    ///< C# type with the static, e.g. "Arche.Foo.Entry".
    char    methodName[64];   ///< C# method name, e.g. "Init", "Tick".
    int32_t ok;               ///< OUT: 1 = found+executed, 0 = failed.
};

// ----------------------------------------------------------------------------
// Method hashes — MUST match engine-side `fnv1aHashConst("archetyped:csharp:*")`
// ----------------------------------------------------------------------------
/// @brief `archetyped:csharp:registerDomain` FNV-1a hash.
constexpr uint32_t arche_csharp_registerDomainHash    = fnv1aHashConst("archetyped:csharp:registerDomain");
/// @brief `archetyped:csharp:registerContext` FNV-1a hash.
constexpr uint32_t arche_csharp_registerContextHash   = fnv1aHashConst("archetyped:csharp:registerContext");
/// @brief `archetyped:csharp:unregisterContext` FNV-1a hash.
constexpr uint32_t arche_csharp_unregisterContextHash = fnv1aHashConst("archetyped:csharp:unregisterContext");
/// @brief `archetyped:csharp:callStatic` FNV-1a hash.
constexpr uint32_t arche_csharp_callStaticHash        = fnv1aHashConst("archetyped:csharp:callStatic");

// CLR SchemaInstance — reuses SchemaInstancePayload but routed through ClrFactory
/// @brief `archetyped:clr:registerStruct` FNV-1a hash.
constexpr uint32_t arche_clr_registerStructHash = fnv1aHashConst("archetyped:clr:registerStruct");
/// @brief `archetyped:clr:dumpSchema` FNV-1a hash.
constexpr uint32_t arche_clr_dumpSchemaHash     = fnv1aHashConst("archetyped:clr:dumpSchema");
/// @brief `archetyped:clr:compose` FNV-1a hash.
constexpr uint32_t arche_clr_composeHash        = fnv1aHashConst("archetyped:clr:compose");
/// @brief `archetyped:clr:createContext` FNV-1a hash.
constexpr uint32_t arche_clr_createContextHash  = fnv1aHashConst("archetyped:clr:createContext");
/// @brief `archetyped:clr:destroyContext` FNV-1a hash.
constexpr uint32_t arche_clr_destroyContextHash = fnv1aHashConst("archetyped:clr:destroyContext");

/**
 * @struct arche_clr_compose_ctx
 * @brief Payload for `archetyped:clr:compose` — merges SchemaInstance + ALC import.
 */
struct arche_clr_compose_ctx {
    uint32_t targetId;   ///< FNV-1a of target domain name.
    uint32_t importId;   ///< FNV-1a of import domain name.
};

/**
 * @struct arche_csharp_getFunc_ctx
 * @brief Legacy in/out payload for `archetyped:csharp:getFuncPtr` (used via `outputBuffer`).
 * @note New code uses the slim `In/Out` structs in `CLR.h::bind` directly.
 */
struct arche_csharp_getFunc_ctx {
    char    domain[64];   ///< Target domain id.
    char    typeName[256];///< C# type.
    char    methodName[64];///< C# method.
    void*   outFunc;      ///< OUT: function pointer (Bootstrap.GetFunctionPointer).
    int32_t ok;           ///< OUT: 1 = ok, 0 = not found / error.
};

/**
 * @struct arche_clr_installDirect_ctx
 * @brief Payload for `archetyped:clr:installDirect` — exposes a C/C++ fnPtr to C#.
 */
struct arche_clr_installDirect_ctx {
    char     methodName[64]; ///< Name under which C# sees the function.
    uint32_t methodHash;     ///< FNV-1a of `methodName` (key in ankerl map).
    void*    fnPtr;          ///< Native function pointer.
    uint32_t retType;        ///< SCHEMA_* return type.
    uint32_t argCount;       ///< Number of args, max 8.
    uint32_t argTypes[8];    ///< SCHEMA_* per arg.
};

/// @brief `archetyped:csharp:getFuncPtr` FNV-1a hash.
constexpr uint32_t arche_csharp_getFuncHash      = fnv1aHashConst("archetyped:csharp:getFuncPtr");
/// @brief `archetyped:clr:installDirect` FNV-1a hash.
constexpr uint32_t arche_clr_installDirectHash   = fnv1aHashConst("archetyped:clr:installDirect");
/// @brief `archetyped:clr:addImport` FNV-1a hash.
constexpr uint32_t arche_clr_addImportHash       = fnv1aHashConst("archetyped:clr:addImport");

} // namespace FractalSDK

// --- Global compat: engine/modules using ::arche_* without namespace ---
using arche_csharp_register_ctx    = FractalSDK::arche_csharp_register_ctx;
using arche_csharp_unregister_ctx  = FractalSDK::arche_csharp_unregister_ctx;
using arche_csharp_call_static_ctx = FractalSDK::arche_csharp_call_static_ctx;
using arche_clr_compose_ctx        = FractalSDK::arche_clr_compose_ctx;
using arche_csharp_getFunc_ctx     = FractalSDK::arche_csharp_getFunc_ctx;
using arche_clr_installDirect_ctx  = FractalSDK::arche_clr_installDirect_ctx;
constexpr uint32_t arche_csharp_registerDomainHash    = FractalSDK::arche_csharp_registerDomainHash;
constexpr uint32_t arche_csharp_registerContextHash   = FractalSDK::arche_csharp_registerContextHash;
constexpr uint32_t arche_csharp_unregisterContextHash = FractalSDK::arche_csharp_unregisterContextHash;
constexpr uint32_t arche_csharp_callStaticHash        = FractalSDK::arche_csharp_callStaticHash;
constexpr uint32_t arche_clr_registerStructHash = FractalSDK::arche_clr_registerStructHash;
constexpr uint32_t arche_clr_dumpSchemaHash     = FractalSDK::arche_clr_dumpSchemaHash;
constexpr uint32_t arche_clr_composeHash        = FractalSDK::arche_clr_composeHash;
constexpr uint32_t arche_clr_createContextHash  = FractalSDK::arche_clr_createContextHash;
constexpr uint32_t arche_clr_destroyContextHash = FractalSDK::arche_clr_destroyContextHash;
constexpr uint32_t arche_csharp_getFuncHash     = FractalSDK::arche_csharp_getFuncHash;
constexpr uint32_t arche_clr_installDirectHash  = FractalSDK::arche_clr_installDirectHash;
constexpr uint32_t arche_clr_addImportHash      = FractalSDK::arche_clr_addImportHash;
