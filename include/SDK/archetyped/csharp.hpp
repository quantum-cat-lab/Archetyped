#pragma once
// csharp.hpp — compat shim, forwards to CLR.h manager.
// New modules: #include <SDK/archetyped/CLR.h> (class CLR like ECS).
#include <SDK/archetyped/CLR.h>
#include <SDK/archetyped/csharp.h>
#include <SDK/archetyped/schema/SchemaInstancePayload.h>
#include <SDK/archetyped/schema/SchemaBlock.h>
#include <vector>
#include <string>

namespace FractalSDK {
namespace CLRCompatFree {
inline bool createContext(const char* d,const char* p,const char* e){ if(!d||!p||!e) return false; arche_csharp_register_ctx ctx{}; std::snprintf(ctx.domain,sizeof(ctx.domain),"%s",d); std::snprintf(ctx.dllPath,sizeof(ctx.dllPath),"%s",p); std::snprintf(ctx.entryType,sizeof(ctx.entryType),"%s",e); FURCMDPacket pkt{}; pkt.methodHash=arche_csharp_registerContextHash; pkt.payloadSize=sizeof(ctx); pkt.payload=&ctx; auto* s=SDK::Get(); if(!s) return false; s->sendPacket(pkt); return true; }
inline bool callStatic(const char* d,const char* t,const char* m){ if(!d||!t||!m) return false; arche_csharp_call_static_ctx ctx{}; std::snprintf(ctx.domain,sizeof(ctx.domain),"%s",d); std::snprintf(ctx.typeName,sizeof(ctx.typeName),"%s",t); std::snprintf(ctx.methodName,sizeof(ctx.methodName),"%s",m); FURCMDPacket pkt{}; pkt.methodHash=arche_csharp_callStaticHash; pkt.payloadSize=sizeof(ctx); pkt.payload=&ctx; auto* s=SDK::Get(); if(!s) return false; s->sendPacket(pkt); return true; }
}
}
namespace arche::csharp {
    inline bool createContext(const char* d,const char* p,const char* e){ return FractalSDK::CLRCompatFree::createContext(d,p,e); }
    inline bool callStatic(const char* d,const char* t,const char* m){ return FractalSDK::CLRCompatFree::callStatic(d,t,m); }
}
