#include "BridgeRegistry.h"
#include "../JvmRegistry/JvmFactory.h"
#include "../ClrRegistry/ClrFactory.h"
#include "../SchemaRegistry/SchemaFactory.h"
#include "../../core/FractalKernel.h"
#include <SDK/archetyped/schema/SchemaBlock.h>
#include <jni.h>
#include <atomic>
#include <thread>
#include <chrono>

BridgeRegistry& BridgeRegistry::instance() { static BridgeRegistry inst; return inst; }

std::string BridgeRegistry::jniSig(uint32_t typeId) {
    switch (typeId) {
        case SCHEMA_VOID: return "V";
        case SCHEMA_BOOL: return "Z";
        case SCHEMA_U8: case SCHEMA_I8: return "B";
        case SCHEMA_U16: case SCHEMA_I16: return "S";
        case SCHEMA_U32: case SCHEMA_I32: return "I";
        case SCHEMA_U64: case SCHEMA_I64: return "J";
        case SCHEMA_F32: return "F";
        case SCHEMA_F64: return "D";
        case SCHEMA_PTR: case SCHEMA_HANDLE: case SCHEMA_STRUCT: return "J";
        default: return "J";
    }
}
std::string BridgeRegistry::methodSig(uint32_t retType, std::span<const uint32_t> argTypes) {
    std::string s="("; for(auto t: argTypes) s+=jniSig(t); s+=")"; s+=jniSig(retType); return s;
}
std::string BridgeRegistry::clrSig(uint32_t typeId) { return jniSig(typeId); }
std::string BridgeRegistry::csharpType(uint32_t typeId) {
    switch(typeId){
        case SCHEMA_VOID: return "void";
        case SCHEMA_BOOL: return "bool";
        case SCHEMA_I8: return "sbyte"; case SCHEMA_U8: return "byte";
        case SCHEMA_I16: return "short"; case SCHEMA_U16: return "ushort";
        case SCHEMA_I32: return "int"; case SCHEMA_U32: return "uint";
        case SCHEMA_I64: return "long"; case SCHEMA_U64: return "ulong";
        case SCHEMA_F32: return "float"; case SCHEMA_F64: return "double";
        case SCHEMA_PTR: return "nint"; case SCHEMA_HANDLE: return "nint";
        case SCHEMA_STRUCT: return "nint";
        default: return "nint";
    }
}

void BridgeRegistry::installDirect(uint32_t ctxId, const char* jClass, const char* jMethod, uint32_t retType, std::span<const uint32_t> argTypes, void* fnPtr){
    std::string sig = methodSig(retType, argTypes);
    JvmFactory::instance().registerNative(ctxId, jClass, jMethod, sig.c_str(), fnPtr);
}

static jint bridge_nCallFURCMD(JNIEnv*, jclass, jint hash, jlong payloadAddr, jint payloadSize){
    std::atomic<uint64_t> fence{0};
    FURCMDPacket pkt{}; pkt.methodHash=(uint32_t)hash; pkt.payload=(void*)(uintptr_t)payloadAddr; pkt.payloadSize=(uint16_t)payloadSize; pkt.fence=(uint64_t*)&fence;
    FractalKernel::instance().sendCMDPacket(pkt);
    auto start=std::chrono::steady_clock::now();
    while(fence.load(std::memory_order_acquire)==0){ if(std::chrono::steady_clock::now()-start>std::chrono::seconds(5)) return -1; std::this_thread::sleep_for(std::chrono::microseconds(50)); }
    return 0;
}
static jint bridge_nCallFURCMDWithOutput(JNIEnv*, jclass, jint hash, jlong payloadAddr, jint payloadSize, jlong outAddr, jint outSize){
    std::atomic<uint64_t> fence{0};
    FURCMDPacket pkt{}; pkt.methodHash=(uint32_t)hash; pkt.payload=payloadAddr?(void*)(uintptr_t)payloadAddr:nullptr; pkt.payloadSize=(uint16_t)payloadSize; pkt.outputBuffer=outAddr?(void*)(uintptr_t)outAddr:nullptr; pkt.fence=(uint64_t*)&fence; (void)outSize;
    FractalKernel::instance().sendCMDPacket(pkt);
    auto start=std::chrono::steady_clock::now();
    while(fence.load(std::memory_order_acquire)==0){ if(std::chrono::steady_clock::now()-start>std::chrono::seconds(5)) return -1; std::this_thread::sleep_for(std::chrono::microseconds(50)); }
    return 0;
}
void BridgeRegistry::installFURCMDBridge(uint32_t ctxId, const char* jClass){
    JvmFactory::instance().registerNative(ctxId, jClass, "nCallFURCMD", "(IJI)I", reinterpret_cast<void*>(&bridge_nCallFURCMD));
    JvmFactory::instance().registerNative(ctxId, jClass, "nCallFURCMDWithOutput", "(IJIJI)I", reinterpret_cast<void*>(&bridge_nCallFURCMDWithOutput));
}
void BridgeRegistry::installAll(uint32_t ctxId, const char* bridgeClass){ installFURCMDBridge(ctxId, bridgeClass); }

extern "C" int32_t clr_nCallFURCMD(int32_t hash, intptr_t payloadAddr, int32_t payloadSize){
    std::atomic<uint64_t> fence{0};
    FURCMDPacket pkt{}; pkt.methodHash=(uint32_t)hash; pkt.payload=(void*)payloadAddr; pkt.payloadSize=(uint16_t)payloadSize; pkt.fence=(uint64_t*)&fence;
    FractalKernel::instance().sendCMDPacket(pkt);
    auto start=std::chrono::steady_clock::now();
    while(fence.load(std::memory_order_acquire)==0){ if(std::chrono::steady_clock::now()-start>std::chrono::seconds(5)) return -1; std::this_thread::sleep_for(std::chrono::microseconds(50)); }
    return 0;
}
extern "C" int32_t clr_nCallFURCMDWithOutput(int32_t hash, intptr_t payloadAddr, int32_t payloadSize, intptr_t outAddr, int32_t outSize){
    std::atomic<uint64_t> fence{0};
    FURCMDPacket pkt{}; pkt.methodHash=(uint32_t)hash; pkt.payload=payloadAddr?(void*)payloadAddr:nullptr; pkt.payloadSize=(uint16_t)payloadSize; pkt.outputBuffer=outAddr?(void*)outAddr:nullptr; pkt.fence=(uint64_t*)&fence; (void)outSize;
    FractalKernel::instance().sendCMDPacket(pkt);
    auto start=std::chrono::steady_clock::now();
    while(fence.load(std::memory_order_acquire)==0){ if(std::chrono::steady_clock::now()-start>std::chrono::seconds(5)) return -1; std::this_thread::sleep_for(std::chrono::microseconds(50)); }
    return 0;
}
extern "C" void BridgeRegistry_installDirectClr_hash(uint32_t methodHash, void* fnPtr){
    BridgeRegistry::instance().installDirectClr(0, methodHash, fnPtr);
}

void BridgeRegistry::installDirectClr(uint32_t, const char* methodName, uint32_t, std::span<const uint32_t>, void* fnPtr){
    if(!methodName||!fnPtr) return;
    uint32_t h = fnv1aHash(methodName);
    std::lock_guard<std::mutex> lk(clrMutex_);
    clrDirectFns_[h] = fnPtr;
}
void BridgeRegistry::installDirectClr(uint32_t, uint32_t methodHash, void* fnPtr){
    if(!methodHash||!fnPtr) return;
    std::lock_guard<std::mutex> lk(clrMutex_);
    clrDirectFns_[methodHash] = fnPtr;
}
void BridgeRegistry::installFURCMDBridgeClr(uint32_t, const char*){
    uint32_t hk = fnv1aHash("nCallFURCMD");
    uint32_t hk2 = fnv1aHash("nCallFURCMDWithOutput");
    std::lock_guard<std::mutex> lk(clrMutex_);
    clrDirectFns_[hk] = reinterpret_cast<void*>(&clr_nCallFURCMD);
    clrDirectFns_[hk2] = reinterpret_cast<void*>(&clr_nCallFURCMDWithOutput);
}
void* BridgeRegistry::getClrDirectFn(const char* methodName){
    if(!methodName) return nullptr;
    return getClrDirectFn(fnv1aHash(methodName));
}
void* BridgeRegistry::getClrDirectFn(uint32_t methodHash){
    if(!methodHash) return nullptr;
    std::lock_guard<std::mutex> lk(clrMutex_);
    auto it=clrDirectFns_.find(methodHash);
    return it==clrDirectFns_.end()?nullptr:it->second;
}
void* BridgeRegistry::getClrFURCMDFn(){ return reinterpret_cast<void*>(&clr_nCallFURCMD); }
void* BridgeRegistry::getClrFURCMDWithOutputFn(){ return reinterpret_cast<void*>(&clr_nCallFURCMDWithOutput); }
void BridgeRegistry::clearClrDirect(uint32_t domainHash){
    std::lock_guard<std::mutex> lk(clrMutex_);
    if (domainHash == 0) clrDirectFns_.clear();
    else clrDirectFns_.clear(); // TODO: per-domain bucket if hot path shows churn
}
void BridgeRegistry::clearAllClrDirect(){ std::lock_guard<std::mutex> lk(clrMutex_); clrDirectFns_.clear(); }
