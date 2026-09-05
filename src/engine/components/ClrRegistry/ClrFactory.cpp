#include "ClrFactory.h"
#include "../../core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <SDK/archetyped/schema/SchemaRegistryPayload.h>
#include <cstdio>
#include <cstring>
namespace {
constexpr uint32_t kCreate = fnv1aHashConst("archetyped:clr:createInstance");
constexpr uint32_t kDestroy = fnv1aHashConst("archetyped:clr:destroyInstance");
constexpr uint32_t kCompose = fnv1aHashConst("archetyped:clr:compose");
constexpr uint32_t kDomain = fnv1aHashConst("archetyped:clr:registerInstance");
constexpr uint32_t kRegisterSchema = fnv1aHashConst("archetyped:clr:registerSchema");
constexpr uint32_t kDumpSchema = fnv1aHashConst("archetyped:clr:dumpSchema");
}
ClrFactory& ClrFactory::instance(){ static ClrFactory s; return s; }
void ClrFactory::registerClrDomain(FURCMDPacket&){
    FractalKernel::instance().registerCMDMethod(kCreate, createContextCMD);
    FractalKernel::instance().registerCMDMethod(kDestroy, destroyContextCMD);
    FractalKernel::instance().registerCMDMethod(kCompose, composeCMD);
    FractalKernel::instance().registerCMDMethod(kRegisterSchema, registerSchemaCMD);
    FractalKernel::instance().registerCMDMethod(kDumpSchema, dumpSchemaCMD);
    std::fprintf(stderr,"[ClrFactory] domain registered (clr + SchemaInstance)\n");
}
void ClrFactory::createContextCMD(FURCMDPacket& pkt){ auto* c=(clrCreateInstanceCtx*)pkt.payload; if(!c) return; c->outContextId=instance().createContext(c->domain, c->dllPath); }
void ClrFactory::destroyContextCMD(FURCMDPacket& pkt){ auto* c=(clrDestroyInstanceCtx*)pkt.payload; if(!c) return; instance().destroyContext(c->contextId); }
void ClrFactory::composeCMD(FURCMDPacket& pkt){ auto* c=(clrComposeInstanceCtx*)pkt.payload; if(!c) return; instance().compose(c->targetId,c->importId); }
void ClrFactory::registerSchemaCMD(FURCMDPacket& pkt){
    auto* c = static_cast<SchemaRegisterSchemaPayload*>(pkt.payload);
    if (!c || !pkt.payloadSize) return;
    std::vector<std::pair<uint32_t, std::string>> fields;
    fields.reserve(c->fieldCount);
    for (uint32_t i=0;i<c->fieldCount;++i) fields.emplace_back(c->fields[i].typeId, c->fields[i].name);
    const char* domain = c->domain[0] ? c->domain : "clr";
    instance().registerSchema(domain, c->name, fields);
    std::fprintf(stderr,"[ClrFactory][Schema] domain='%s' schema='%s' fields=%u\n", domain, c->name, c->fieldCount);
}
void ClrFactory::dumpSchemaCMD(FURCMDPacket& pkt){
    auto* p = static_cast<SchemaDumpSchemaInstancePayload*>(pkt.payload);
    if (!p || !p->outputBuffer) return;
    auto buf = instance().dumpSchema(p->domain[0] ? p->domain : "clr");
    p->outSize = 0;
    if (!buf.empty() && buf.size() <= p->bufferSize) {
        std::memcpy(p->outputBuffer, buf.data(), buf.size());
        p->outSize = static_cast<uint32_t>(buf.size());
    }
    if (pkt.fence) *pkt.fence = 1;
    if (p->fence) *p->fence = 1;
}
uint32_t ClrFactory::createContext(const char* domain, const char* dllPath){
    if(!domain||!domain[0]) return 0;
    uint32_t id = fnv1aHash(domain);
    std::lock_guard<std::mutex> lk(m_);
    auto it=ctxs_.find(id); if(it!=ctxs_.end()) return id;
    ClrInstance cx; cx.id=id; cx.domain=domain; cx.dllPath=dllPath?dllPath:"";
    ctxs_.emplace(id, std::move(cx));
    if (schemaRegs_.find(id)==schemaRegs_.end()) {
        schemaRegs_.emplace(id, std::make_unique<SchemaInstance>());
    }
    std::fprintf(stderr,"[ClrFactory] create '%s' -> %08x dll='%s' + SchemaInstance\n", domain, id, dllPath?dllPath:"");
    return id;
}
void ClrFactory::destroyContext(uint32_t id){
    std::lock_guard<std::mutex> lk(m_);
    auto it=ctxs_.find(id);
    if(it!=ctxs_.end()){
        std::fprintf(stderr,"[ClrFactory] destroy %08x '%s' + SchemaInstance\n", id, it->second.domain.c_str());
        ctxs_.erase(it);
    }
    auto tit = schemaRegs_.find(id);
    if(tit!=schemaRegs_.end()) schemaRegs_.erase(tit);
    // remove from other import lists
    for(auto& kv : ctxs_){
        auto& v = kv.second.imports;
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
    }
}
void ClrFactory::compose(uint32_t target, uint32_t importId){
    std::string targetDomain, importDomain;
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it=ctxs_.find(target); auto jt=ctxs_.find(importId);
        if(it==ctxs_.end()||jt==ctxs_.end()) return;
        if (std::find(it->second.imports.begin(), it->second.imports.end(), importId)==it->second.imports.end())
            it->second.imports.push_back(importId);
        targetDomain = it->second.domain;
        importDomain = jt->second.domain;
        auto tit = schemaRegs_.find(target);
        auto iit = schemaRegs_.find(importId);
        if(tit!=schemaRegs_.end() && iit!=schemaRegs_.end()){
            tit->second->mergeFrom(*iit->second);
            std::fprintf(stderr,"[ClrFactory] compose types %08x <- %08x\n", target, importId);
        }
    }
    auto addImport = ClrHost::instance().getBootstrapAddImport();
    if (addImport && !targetDomain.empty() && !importDomain.empty()) {
        int rc = addImport(targetDomain.c_str(), importDomain.c_str());
        std::fprintf(stderr,"[ClrFactory] Bootstrap AddImport '%s' -> '%s' rc=%d\n", targetDomain.c_str(), importDomain.c_str(), rc);
    }
}
void ClrFactory::registerSchema(std::string_view domain, std::string_view name, const std::vector<std::pair<uint32_t, std::string>>& fields){
    uint32_t h = fnv1aHash(domain);
    std::lock_guard<std::mutex> lk(m_);
    auto it = schemaRegs_.find(h);
    if(it==schemaRegs_.end()){
        auto reg = std::make_unique<SchemaInstance>();
        reg->registerSchema(name, fields);
        schemaRegs_.emplace(h, std::move(reg));
    } else {
        it->second->registerSchema(name, fields);
    }
}
const SchemaDef* ClrFactory::getSchema(std::string_view domain, uint32_t hash) const {
    std::lock_guard<std::mutex> lk(m_);
    uint32_t h = fnv1aHash(domain);
    auto it = schemaRegs_.find(h);
    if(it==schemaRegs_.end()) return nullptr;
    return it->second->getSchema(hash);
}
const SchemaDef* ClrFactory::getSchema(std::string_view domain, std::string_view name) const {
    return getSchema(domain, fnv1aHash(name));
}
uint32_t ClrFactory::getSchemaFieldOffset(std::string_view domain, uint32_t schemaHash, uint64_t fieldHash) const {
    std::lock_guard<std::mutex> lk(m_);
    uint32_t h = fnv1aHash(domain);
    auto it = schemaRegs_.find(h);
    if(it==schemaRegs_.end()) return 0;
    return it->second->getSchemaFieldOffset(schemaHash, fieldHash);
}
std::vector<uint8_t> ClrFactory::dumpSchema(std::string_view domain) const {
    std::lock_guard<std::mutex> lk(m_);
    uint32_t h = fnv1aHash(domain);
    auto it = schemaRegs_.find(h);
    if(it==schemaRegs_.end()) return {};
    return it->second->dumpSchema();
}
void ClrFactory::composeTypes(std::string_view target, std::string_view importDomain){
    uint32_t th = fnv1aHash(target);
    uint32_t ih = fnv1aHash(importDomain);
    if(th==ih) return;
    std::lock_guard<std::mutex> lk(m_);
    auto tit = schemaRegs_.find(th);
    auto iit = schemaRegs_.find(ih);
    if(tit==schemaRegs_.end()||iit==schemaRegs_.end()) return;
    tit->second->mergeFrom(*iit->second);
}
bool ClrFactory::hasTypes(std::string_view domain) const {
    std::lock_guard<std::mutex> lk(m_);
    return schemaRegs_.find(fnv1aHash(domain))!=schemaRegs_.end();
}
void ClrFactory::invalidateTypes(std::string_view domain){
    std::lock_guard<std::mutex> lk(m_);
    schemaRegs_.erase(fnv1aHash(domain));
}
void ClrFactory::submit(uint32_t, std::function<void()> task){ sched_.submit(ClrTask{std::move(task)}); }
std::string ClrFactory::getDllPath(uint32_t id){ std::lock_guard<std::mutex> lk(m_); auto it=ctxs_.find(id); return it==ctxs_.end()?"":it->second.dllPath; }