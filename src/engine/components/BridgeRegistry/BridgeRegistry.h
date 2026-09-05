#pragma once
#include <cstdint>
#include <string>
#include <span>
#include <vector>
#include <mutex>
#include "ankerl/unordered_dense.h"

class BridgeRegistry {
public:
    static BridgeRegistry& instance();

    static std::string jniSig(uint32_t typeId);
    static std::string methodSig(uint32_t retType, std::span<const uint32_t> argTypes);

    static std::string clrSig(uint32_t typeId);
    static std::string csharpType(uint32_t typeId);

    void installDirect(uint32_t ctxId, const char* jClass, const char* jMethod,
                       uint32_t retType, std::span<const uint32_t> argTypes, void* fnPtr);

    void installFURCMDBridge(uint32_t ctxId, const char* jClass);
    void installAll(uint32_t ctxId, const char* bridgeClass);

    // C++ -> C# direct: store fnPtr by 32-bit hash (fnv1a)
    void installDirectClr(uint32_t ctxId, const char* methodName,
                          uint32_t retType, std::span<const uint32_t> argTypes, void* fnPtr);
    void installDirectClr(uint32_t ctxId, uint32_t methodHash, void* fnPtr);
    void installFURCMDBridgeClr(uint32_t ctxId, const char* typeName);
    void* getClrDirectFn(const char* methodName);
    void* getClrDirectFn(uint32_t methodHash);
    void* getClrFURCMDFn();
    void* getClrFURCMDWithOutputFn();
    void clearClrDirect(uint32_t ctxId);
    void clearAllClrDirect();

private:
    BridgeRegistry() = default;
    ankerl::unordered_dense::map<uint32_t, void*> clrDirectFns_;
    std::mutex clrMutex_;
};
