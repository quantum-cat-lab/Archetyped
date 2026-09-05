#pragma once
/// @file jvm.h
/// @brief JVM bridge SDK — C-compatible POD layer for FURCMD payloads.
/// @details Module never includes `jni.h`. All JNI types are opaque (`void*`).
///          Structures are POD with fixed layout so FURCMD can memcpy them as payload.
/// @see CLR.h for the high-level manager. @see csharp.h for the C# mirror.
/// @code
///   // C++ side usage:
///   FractalSDK::arche_jvm_create_ctx ctx{};
///   std::snprintf(ctx.name, 64, "MyJvm");
///   std::snprintf(ctx.jarPath, 1024, "mod://arche-core.jar");
///   packet.methodHash = FractalSDK::arche_jvm_createContextHash;
///   packet.payload = &ctx;
/// @endcode

#include <SDK/archetyped/hash/hash.h>
#include <cstdint>

namespace FractalSDK {

/**
 * @struct arche_jvm_call
 * @brief Opaque native call context. The engine fills it via a per-signature trampoline.
 */
struct arche_jvm_call {
    void*    env;        ///< `JNIEnv*` (opaque to module).
    void*    thiz;       ///< `jobject` / `jclass` (opaque to module).
    void*    args;       ///< Packed argument buffer (signature-specific layout).
    void*    result;     ///< Module writes its return value here.
    uint32_t sigId;      ///< Which signature trampoline dispatched this call.
};

/// @brief Type-erased native function pointer called by JVM through JNI trampoline.
typedef void (*arche_jvm_native_fn)(arche_jvm_call* call);

/**
 * @struct arche_jvm_create_ctx
 * @brief Payload for `archetyped:jvm:createContext` — creates a JVM domain + URLClassLoader.
 */
struct arche_jvm_create_ctx {
    char     name[64];     ///< Domain id, e.g. "MyJvm". Hashed FNV-1a.
    char     jarPath[1024];///< VFS `mod://...` or absolute path to .jar.
    uint32_t outContextId; ///< OUT: assigned context id (0 = failure).
};

/**
 * @struct arche_jvm_destroy_ctx
 * @brief Payload for `archetyped:jvm:destroyContext` — unloads URLClassLoader + drops SchemaInstance.
 */
struct arche_jvm_destroy_ctx {
    uint32_t contextId;    ///< Target context id.
};

/**
 * @struct arche_jvm_compose_ctx
 * @brief Payload for `archetyped:jvm:compose` — merges SchemaInstance + adds import.
 */
struct arche_jvm_compose_ctx {
    uint32_t targetId;     ///< FNV-1a of target domain.
    uint32_t importId;     ///< FNV-1a of import domain.
};

/**
 * @struct arche_jvm_register_native_ctx
 * @brief Payload for `archetyped:jvm:registerNative` — exposes C/C++ fn to JVM.
 */
struct arche_jvm_register_native_ctx {
    uint32_t            contextId;    ///< Target context id.
    char                className[256];///< Fully-qualified Java class.
    char                methodName[64];///< Java method name.
    char                signature[128];///< JNI signature, e.g. "(I)I".
    arche_jvm_native_fn fnPtr;        ///< Native function pointer.
};

/**
 * @struct arche_jvm_call_static_ctx
 * @brief Payload for `archetyped:jvm:callStatic` — fire-and-forget static call.
 */
struct arche_jvm_call_static_ctx {
    uint32_t contextId;        ///< Target context id.
    char     className[256];   ///< Fully-qualified Java class.
    char     methodName[64];   ///< Static method name.
    char     signature[128];   ///< JNI signature.
    uint8_t  argData[256];     ///< Packed arguments.
    uint32_t argSize;          ///< Size of `argData` in bytes.
};

/// @brief `archetyped:jvm:registerDomain` FNV-1a hash.
constexpr uint32_t arche_jvm_registerDomainHash = fnv1aHashConst("archetyped:jvm:registerDomain");
/// @brief `archetyped:jvm:createContext` FNV-1a hash.
constexpr uint32_t arche_jvm_createContextHash  = fnv1aHashConst("archetyped:jvm:createContext");
/// @brief `archetyped:jvm:destroyContext` FNV-1a hash.
constexpr uint32_t arche_jvm_destroyContextHash = fnv1aHashConst("archetyped:jvm:destroyContext");
/// @brief `archetyped:jvm:compose` FNV-1a hash.
constexpr uint32_t arche_jvm_composeHash        = fnv1aHashConst("archetyped:jvm:compose");
/// @brief `archetyped:jvm:registerNative` FNV-1a hash.
constexpr uint32_t arche_jvm_registerNativeHash = fnv1aHashConst("archetyped:jvm:registerNative");
/// @brief `archetyped:jvm:callStatic` FNV-1a hash.
constexpr uint32_t arche_jvm_callStaticHash     = fnv1aHashConst("archetyped:jvm:callStatic");

} // namespace FractalSDK

// --- Global compat ---
using arche_jvm_call               = FractalSDK::arche_jvm_call;
using arche_jvm_native_fn          = FractalSDK::arche_jvm_native_fn;
using arche_jvm_create_ctx         = FractalSDK::arche_jvm_create_ctx;
using arche_jvm_destroy_ctx        = FractalSDK::arche_jvm_destroy_ctx;
using arche_jvm_compose_ctx        = FractalSDK::arche_jvm_compose_ctx;
using arche_jvm_register_native_ctx= FractalSDK::arche_jvm_register_native_ctx;
using arche_jvm_call_static_ctx    = FractalSDK::arche_jvm_call_static_ctx;
constexpr uint32_t arche_jvm_registerDomainHash = FractalSDK::arche_jvm_registerDomainHash;
constexpr uint32_t arche_jvm_createContextHash  = FractalSDK::arche_jvm_createContextHash;
constexpr uint32_t arche_jvm_destroyContextHash = FractalSDK::arche_jvm_destroyContextHash;
constexpr uint32_t arche_jvm_composeHash        = FractalSDK::arche_jvm_composeHash;
constexpr uint32_t arche_jvm_registerNativeHash = FractalSDK::arche_jvm_registerNativeHash;
constexpr uint32_t arche_jvm_callStaticHash     = FractalSDK::arche_jvm_callStaticHash;
