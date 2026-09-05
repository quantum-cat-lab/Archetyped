#pragma once
#include "../ClrScheduler/ClrScheduler.h"
#include "../SchemaRegistry/SchemaInstance.h"
#include "FURCMD/FURCMD.h"
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include "ankerl/unordered_dense.h"

struct clrCreateInstanceCtx { char domain[64]; char dllPath[1024]; uint32_t outContextId; };
struct clrDestroyInstanceCtx { uint32_t contextId; };
struct clrComposeInstanceCtx { uint32_t targetId; uint32_t importId; };

struct ClrInstance { uint32_t id=0; std::string domain; std::string dllPath; std::vector<uint32_t> imports; };

class ClrFactory {
public:
    static ClrFactory& instance();
    static void registerClrDomain(FURCMDPacket& pkt);
    static void createContextCMD(FURCMDPacket& pkt);
    static void destroyContextCMD(FURCMDPacket& pkt);
    static void composeCMD(FURCMDPacket& pkt);
    static void registerSchemaCMD(FURCMDPacket& pkt);
    static void dumpSchemaCMD(FURCMDPacket& pkt);

    uint32_t createContext(const char* domain, const char* dllPath);
    void destroyContext(uint32_t id);
    void compose(uint32_t target, uint32_t importId);

    // SchemaInstance part — per-domain, bound to CLR domain (like in Jvm Registry 'same lol' )
    void registerSchema(std::string_view domain, std::string_view name, const std::vector<std::pair<uint32_t, std::string>>& fields);
    const SchemaDef* getSchema(std::string_view domain, uint32_t hash) const;
    const SchemaDef* getSchema(std::string_view domain, std::string_view name) const;
    uint32_t getSchemaFieldOffset(std::string_view domain, uint32_t schemaHash, uint64_t fieldHash) const;
    std::vector<uint8_t> dumpSchema(std::string_view domain) const;
    void composeTypes(std::string_view target, std::string_view importDomain);
    bool hasTypes(std::string_view domain) const;
    void invalidateTypes(std::string_view domain);

    void submit(uint32_t ctxId, std::function<void()> task);
    std::string getDllPath(uint32_t ctxId);
private:
    ClrFactory()=default;
    ClrFactory(const ClrFactory&)=delete;
    ClrFactory& operator=(const ClrFactory&)=delete;
    ankerl::unordered_dense::map<uint32_t, ClrInstance> ctxs_;
    ankerl::unordered_dense::map<uint32_t, std::unique_ptr<SchemaInstance>> schemaRegs_;
    mutable std::mutex m_;
    ClrScheduler& sched_ = ClrScheduler::instance();
};