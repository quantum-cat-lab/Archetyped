#include "CSharpFactory.h"
#include "../ClrRegistry/ClrFactory.h"
#include "../ClrScheduler/ClrScheduler.h"
#include "../ClrHost/ClrHost.h"
#include "../BridgeRegistry/BridgeRegistry.h"
#include <coreclr_delegates.h>
#include "../SchemaRegistry/SchemaFactory.h"
#include "../vfs/VFS.hpp"
#include "../../core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace { constexpr uint32_t kReg = fnv1aHashConst("archetyped:csharp:registerInstance"); constexpr uint32_t kUnreg = fnv1aHashConst("archetyped:csharp:unregisterInstance"); constexpr uint32_t kDom = fnv1aHashConst("archetyped:csharp:registerInstance"); constexpr uint32_t kCall = fnv1aHashConst("archetyped:csharp:callStatic"); constexpr uint32_t kGetFunc = fnv1aHashConst("archetyped:csharp:getFuncPtr"); constexpr uint32_t kInstall = fnv1aHashConst("archetyped:clr:installDirect"); constexpr uint32_t kAddImport = fnv1aHashConst("archetyped:clr:addImport"); }

CSharpFactory& CSharpFactory::instance(){ static CSharpFactory s; return s; }
uint32_t CSharpFactory::hashDomain(const char* d){ return fnv1aHash(d); }
std::string CSharpFactory::resolveDll(const char* p){
    if(!p||!p[0]) return {};
    if(std::strstr(p,"://")){ char out[VFS::MAX_PATH_LENGTH]{}; if(VFS::Resolve(p,out,sizeof(out))) return out; if(VFS::ResolveFirstExisting(p,out,sizeof(out))) return out;
        const char* slash = std::strrchr(p, '/'); const char* fname = slash ? slash+1 : p;
        if(std::strstr(fname,"://")){ const char* s = std::strstr(fname,"://"); fname = s+3; }
        char probe[VFS::MAX_PATH_LENGTH]{};
        const char* candidates[] = {
            "container/HelloBox/bin/", "container/DemoTest/bin/",
            "/home/mainmasgoose/Documents/PROJECTS/Archetyped/container/HelloBox/bin/",
            "/home/mainmasgoose/Documents/PROJECTS/Archetyped/container/DemoTest/bin/"
        };
        for(auto base: candidates){ std::snprintf(probe,sizeof(probe),"%s%s",base,fname); if(std::filesystem::exists(probe)) return std::filesystem::absolute(probe).string(); }
        return p;
    }
    return p;
}
void CSharpFactory::registerDomain(FURCMDPacket&){ FractalKernel::instance().registerCMDMethod(kReg, registerContextCMD); FractalKernel::instance().registerCMDMethod(kUnreg, unregisterContextCMD); FractalKernel::instance().registerCMDMethod(kCall, callStaticCMD); FractalKernel::instance().registerCMDMethod(kGetFunc, getFuncPtrCMD); FractalKernel::instance().registerCMDMethod(kInstall, installDirectCMD); std::fprintf(stderr,"[CSharpFactory] domain registered\n"); }
void CSharpFactory::registerContextCMD(FURCMDPacket& pkt){ auto* c=(csharpRegisterInstanceCtx*)pkt.payload; if(!c) return; c->outContextId=instance().registerContext(c->domain,c->dllPath,c->entryType); }
void CSharpFactory::unregisterContextCMD(FURCMDPacket& pkt){ auto* c=(csharpUnregisterInstanceCtx*)pkt.payload; if(!c) return; instance().unregisterContext(c->domain); }
void CSharpFactory::callStaticCMD(FURCMDPacket& pkt){ auto* c=(csharpCallStaticInstanceCtx*)pkt.payload; if(!c) return; c->ok = instance().callStatic(c->domain,c->typeName,c->methodName) ? 1 : 0; }
void CSharpFactory::getFuncPtrCMD(FURCMDPacket& pkt){ struct In{ char d[64]; char t[256]; char m[64]; }; struct Out{ void* func; int32_t ok; }; auto* in=(In*)pkt.payload; auto* out=(Out*)pkt.outputBuffer; if(!in) return; void* f=instance().getFuncPtr(in->d,in->t,in->m); if(out){ out->func=f; out->ok=f?1:0; } }
void CSharpFactory::installDirectCMD(FURCMDPacket& pkt){ struct Inst{ char n[64]; uint32_t h; void* p; uint32_t r; uint32_t c; uint32_t a[8]; }; auto* cc=(Inst*)pkt.payload; if(!cc||!cc->p) return; uint32_t h=cc->h?cc->h:fnv1aHash(cc->n); std::fprintf(stderr,"[CSharpFactory] installDirect '%s' hash=%08x fn=%p\n", cc->n, h, cc->p); BridgeRegistry::instance().installDirectClr(0, h, cc->p); }
void CSharpFactory::addImportCMD(FURCMDPacket& pkt){ struct Imp{ uint32_t t; uint32_t i; }; auto* c=(Imp*)pkt.payload; if(!c) return; ClrFactory::instance().compose(c->t,c->i); }

uint32_t CSharpFactory::registerContext(const char* domain, const char* dllPath, const char* entryType){
    if(!domain||!domain[0]) return 0;
    uint32_t id=hashDomain(domain);
    { std::lock_guard<std::mutex> lk(m_); if(ctxs_.find(id)!=ctxs_.end()) return id; }
    std::string resolved=resolveDll(dllPath?dllPath:"");
    const char* dll = resolved.empty()? dllPath : resolved.c_str();
    uint32_t ctxId = ClrFactory::instance().createContext(domain, dll?dll:"");
    if(!ctxId) return 0;
    if(entryType && entryType[0] && ClrHost::instance().isRunning()){
        auto bsCreate = ClrHost::instance().getBootstrapCreate();
        auto bsLoad = ClrHost::instance().getBootstrapLoad();
        auto bsGet = ClrHost::instance().getBootstrapGetFunc();
        if(bsCreate && bsLoad && bsGet && dll){
            int rcc = bsCreate(domain);
            std::fprintf(stderr, "[CSharpFactory] bootstrap CreateALC %s rc=%d\n", domain, rcc);
            int rc = bsLoad(domain, dll);
            if(rc!=0) std::fprintf(stderr,"[CSharpFactory] bootstrap Load %s rc=%d\n", dll, rc);
            else {
                void* func = bsGet(domain, entryType, "Init");
                std::fprintf(stderr,"[CSharpFactory] bootstrap verified %s:%s func=%p\n", dll, entryType, func);
            }
        } else {
            auto fn = ClrHost::instance().getLoadAssemblyFn();
            if(fn && dll){
                void* func=nullptr;
                int rc = fn(dll, entryType, "Init", UNMANAGEDCALLERSONLY_METHOD, nullptr, &func);
                if(rc!=0) std::fprintf(stderr,"[CSharpFactory] warning: load %s:%s.Init rc=%d\n", dll, entryType, rc);
                else std::fprintf(stderr,"[CSharpFactory] verified %s:%s\n", dll, entryType);
            }
        }
    }
    { std::lock_guard<std::mutex> lk(m_); CSharpInstance cx; cx.id=id; cx.domain=domain; cx.dllPath=resolved.empty()?(dllPath?dllPath:""):resolved; cx.entryType=entryType?entryType:""; ctxs_[id]=std::move(cx); }
    std::fprintf(stderr,"[CSharpFactory] context '%s' %08x dll='%s' entry='%s'\n", domain,id, dll?dll:"", entryType?entryType:"");
    return id;
}
bool CSharpFactory::unregisterContext(const char* d){ if(!d||!d[0]) return false; return unregisterContext(hashDomain(d)); }
bool CSharpFactory::unregisterContext(uint32_t h){
    std::string dom; uint32_t id=0;
    { std::lock_guard<std::mutex> lk(m_); auto it=ctxs_.find(h); if(it==ctxs_.end()) return false; dom=it->second.domain; id=it->second.id; ctxs_.erase(it); }
    auto bsUnload = ClrHost::instance().getBootstrapUnload();
    if(bsUnload) bsUnload(dom.c_str());
    BridgeRegistry::instance().clearClrDirect(h);
    ClrFactory::instance().destroyContext(id);
    SchemaFactory::instance().invalidateDomain(dom.c_str());
    std::fprintf(stderr,"[CSharpFactory] unregistered '%s' %08x\n", dom.c_str(), h);
    return true;
}
uint32_t CSharpFactory::findContext(const char* d) const { if(!d||!d[0]) return 0; uint32_t h=hashDomain(d); std::lock_guard<std::mutex> lk(m_); auto it=ctxs_.find(h); return it==ctxs_.end()?0:it->second.id; }
bool CSharpFactory::hasContext(const char* d) const { return findContext(d)!=0; }
void CSharpFactory::submit(const char* d, std::function<void()> t){ if(!d) return; submit(hashDomain(d), std::move(t)); }
void CSharpFactory::submit(uint32_t h, std::function<void()> task){
    CSharpInstance cx; { std::lock_guard<std::mutex> lk(m_); auto it=ctxs_.find(h); if(it==ctxs_.end()) return; cx=it->second; }
    ClrFactory::instance().submit(cx.id, [task=std::move(task)]{ task(); });
}
bool CSharpFactory::callStatic(const char* domain, const char* typeName, const char* methodName){
    if(!domain||!typeName||!methodName) return false;
    uint32_t h=hashDomain(domain);
    { std::lock_guard<std::mutex> lk(m_); if(ctxs_.find(h)==ctxs_.end()) return false; }
    void* func = getFuncPtr(domain, typeName, methodName);
    if(!func) { std::fprintf(stderr,"[CSharpFactory] callStatic %s:%s func=null\n", typeName, methodName); return false; }
    using entry_t = void(*)();
    ((entry_t)func)();
    return true;
}
void* CSharpFactory::getFuncPtr(const char* domain, const char* typeName, const char* methodName){
    if(!domain||!typeName||!methodName) return nullptr;
    uint32_t h=hashDomain(domain);
    { std::lock_guard<std::mutex> lk(m_); if(ctxs_.find(h)==ctxs_.end()) return nullptr; }
    auto bsGet = ClrHost::instance().getBootstrapGetFunc();
    void* func = nullptr;
    if(bsGet) func = bsGet(domain, typeName, methodName);
    else {
        std::string dll; { std::lock_guard<std::mutex> lk(m_); auto it=ctxs_.find(h); if(it==ctxs_.end()) return nullptr; dll=it->second.dllPath; }
        auto fn=ClrHost::instance().getLoadAssemblyFn(); if(!fn||dll.empty()) return nullptr;
        int rc = fn(dll.c_str(), typeName, methodName, UNMANAGEDCALLERSONLY_METHOD, nullptr, &func);
        if(rc!=0||!func) { std::fprintf(stderr,"[CSharpFactory] getFunc %s:%s rc=%d func=%p\n", typeName, methodName, rc, func); return nullptr; }
    }
    return func;
}
