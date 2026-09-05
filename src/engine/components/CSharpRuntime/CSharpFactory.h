#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <mutex>
#include "ankerl/unordered_dense.h"
#include "FURCMD/FURCMD.h"

struct csharpRegisterInstanceCtx { char domain[64]; char dllPath[1024]; char entryType[256]; uint32_t outContextId=0; };
struct csharpUnregisterInstanceCtx { char domain[64]; };
struct csharpCallStaticInstanceCtx { char domain[64]; char typeName[256]; char methodName[64]; int32_t ok=0; };

struct CSharpInstance { uint32_t id=0; std::string domain; std::string dllPath; std::string entryType; };

class CSharpFactory {
public:
    static CSharpFactory& instance();
    static void registerDomain(FURCMDPacket& pkt);
    static void registerContextCMD(FURCMDPacket& pkt);
    static void unregisterContextCMD(FURCMDPacket& pkt);
    static void callStaticCMD(FURCMDPacket& pkt);
    static void getFuncPtrCMD(FURCMDPacket& pkt);
    static void installDirectCMD(FURCMDPacket& pkt);
    static void addImportCMD(FURCMDPacket& pkt);

    uint32_t registerContext(const char* domain, const char* dllPath, const char* entryType);
    bool unregisterContext(const char* domain);
    bool unregisterContext(uint32_t domainHash);
    uint32_t findContext(const char* domain) const;
    bool hasContext(const char* domain) const;
    void submit(const char* domain, std::function<void()> task);
    void submit(uint32_t domainHash, std::function<void()> task);
    bool callStatic(const char* domain, const char* typeName, const char* methodName);
    void* getFuncPtr(const char* domain, const char* typeName, const char* methodName);
private:
    CSharpFactory()=default;
    CSharpFactory(const CSharpFactory&)=delete;
    CSharpFactory& operator=(const CSharpFactory&)=delete;
    static uint32_t hashDomain(const char* d);
    static std::string resolveDll(const char* p);
    ankerl::unordered_dense::map<uint32_t, CSharpInstance> ctxs_;
    mutable std::mutex m_;
};
