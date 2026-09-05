#pragma once
// Lightweight ABI-safe wrapper for FURCMD registration.
// For modules loaded via dlopen — handler MUST be a plain function pointer
//   FURMethod = void(*)(FURCMDPacket&)
// This header only generates static thunks with correct reinterpret_cast,
// so module authors don't write static boilerplate by hand.

#include <SDK/archetyped/FractalSDK.h> // FURCMDPacket, FURMethod

// Free function adaptor: typed payload -> FURCMDPacket
// Usage:
//   void myHandler(MyPayload& p) { ... }
//   api.registerCMDMethod(hash, furMethod<MyPayload, myHandler>());
template <typename PayloadT, void (*Fn)(PayloadT&)>
inline void furThunk(FURCMDPacket& pkt) {
    Fn(*reinterpret_cast<PayloadT*>(pkt.payload));
}

template <typename PayloadT, void (*Fn)(PayloadT&)>
constexpr FURMethod furMethod() noexcept {
    return &furThunk<PayloadT, Fn>;
}

// Same but with outputBuffer passthrough (for handlers that need to write reply)
// PayloadT is input, OutputT is written to pkt.outputBuffer
template <typename PayloadT, typename OutputT, void (*Fn)(PayloadT&, OutputT&)>
inline void furThunkIO(FURCMDPacket& pkt) {
    Fn(*reinterpret_cast<PayloadT*>(pkt.payload),
       *reinterpret_cast<OutputT*>(pkt.outputBuffer));
}

template <typename PayloadT, typename OutputT, void (*Fn)(PayloadT&, OutputT&)>
constexpr FURMethod furMethodIO() noexcept {
    return &furThunkIO<PayloadT, OutputT, Fn>;
}

// Member function adaptor for singleton-style modules.
// Each distinct (T, PayloadT, Method) gets its own static instance slot.
// Set instance before registration; the returned thunk is a plain FURMethod.
//
// Usage in module:
//   struct MyMod { static MyMod& instance(); void handle(Foo& p); };
//   MyMod mod;
//   api.registerCMDMethod(hash, furBindMember<MyMod, Foo, &MyMod::handle>(&mod));
template <typename T, typename PayloadT, void (T::*Method)(PayloadT&)>
struct FurMemberThunk {
    static inline T* instance = nullptr;
    static void invoke(FURCMDPacket& pkt) {
        (instance->*Method)(*reinterpret_cast<PayloadT*>(pkt.payload));
    }
};

template <typename T, typename PayloadT, void (T::*Method)(PayloadT&)>
inline FURMethod furBindMember(T* inst) noexcept {
    FurMemberThunk<T, PayloadT, Method>::instance = inst;
    return &FurMemberThunk<T, PayloadT, Method>::invoke;
}

// KernelAPI (modules) expects void* for registerCMDMethod — cast helper
template <typename PayloadT, void (*Fn)(PayloadT&)>
inline void* furMethodPtr() noexcept {
    return reinterpret_cast<void*>(furMethod<PayloadT, Fn>());
}
template <typename T, typename PayloadT, void (T::*Method)(PayloadT&)>
inline void* furBindMemberPtr(T* inst) noexcept {
    return reinterpret_cast<void*>(furBindMember<T, PayloadT, Method>(inst));
}

// Convenience macro: define + return thunk in one line inside ModuleMain
//   FUR_BIND_MEMBER(hash, MyMod, &MyMod::handle, Foo, &myModInstance)
#define FUR_BIND_MEMBER(Hash, Class, MethodPtr, PayloadT, InstancePtr) \
    ::furBindMember<Class, PayloadT, MethodPtr>(InstancePtr)
