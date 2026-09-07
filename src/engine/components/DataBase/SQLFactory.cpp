#include "SQLFactory.h"
#include "core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include "components/DataBase/SqlInstance.h"
#include <cstdint>

constexpr uint32_t registerSQLDomainHash = fnv1aHashConst("archetyped:sqldb:registerSQLDomain");
constexpr uint32_t openCMDHash = fnv1aHashConst("archetyped:sqldb:openCMD");
constexpr uint32_t executeCMDHash = fnv1aHashConst("archetyped:sqldb:executeCMD");
constexpr uint32_t setStringCMDHash = fnv1aHashConst("archetyped:sqldb:setStringCMD");
constexpr uint32_t getStringCMDHash = fnv1aHashConst("archetyped:sqldb:getStringCMD");
constexpr uint32_t existsCMDHash = fnv1aHashConst("archetyped:sqldb:existsCMD");
constexpr uint32_t closeCMDHash = fnv1aHashConst("archetyped:sqldb:closeCMD");
constexpr uint32_t isOpenCMDHash = fnv1aHashConst("archetyped:sqldb:isOpenCMD");

ankerl::unordered_dense::map<uint32_t, SqlInstance*, IdentityHash> SQLFactory::sqlDomains;

SQLFactory::SQLFactory(){
    FractalKernel::instance().registerCMDMethod(registerSQLDomainHash, &registerSQLDomain);
    FractalKernel::instance().registerCMDMethod(openCMDHash, &openCMD);
    FractalKernel::instance().registerCMDMethod(executeCMDHash, &executeCMD);
    FractalKernel::instance().registerCMDMethod(setStringCMDHash, &setStringCMD);
    FractalKernel::instance().registerCMDMethod(getStringCMDHash, &getStringCMD);
    FractalKernel::instance().registerCMDMethod(existsCMDHash, &existsCMD);
    FractalKernel::instance().registerCMDMethod(closeCMDHash, &closeCMD);
    FractalKernel::instance().registerCMDMethod(isOpenCMDHash, &isOpenCMD);
}
void SQLFactory::registerSQLDomain(FURCMDPacket& packet){
    auto* context = reinterpret_cast<registerSQLDomainContext*>(packet.payload);
    if (sqlDomains.find(context->domainId) == sqlDomains.end()){
        sqlDomains[context->domainId] = new SqlInstance();
    }
}
void SQLFactory::openCMD(FURCMDPacket& packet){
    auto* context = reinterpret_cast<openInstanceCMDContext*>(packet.payload);
    auto it = sqlDomains.find(context->domainId);
    if (it != sqlDomains.end()){
        bool result = it->second->open(context->dbPath);
        *reinterpret_cast<bool*>(packet.outputBuffer) = result;
    }
}
void SQLFactory::executeCMD(FURCMDPacket& packet){
    auto* context = reinterpret_cast<executeInstanceCMDContext*>(packet.payload);
    auto it = sqlDomains.find(context->domainId);
    if (it != sqlDomains.end()){
        bool result = it->second->execute(context->sql);
        *reinterpret_cast<bool*>(packet.outputBuffer) = result;
    }
}
void SQLFactory::setStringCMD(FURCMDPacket& packet){
    auto* context = reinterpret_cast<setStringInstanceCMDContext*>(packet.payload);
    auto it = sqlDomains.find(context->domainId);
    if (it != sqlDomains.end()){
        bool result = it->second->setString(context->key, context->value);
        *reinterpret_cast<bool*>(packet.outputBuffer) = result;
    }
}
void SQLFactory::getStringCMD(FURCMDPacket& packet){
    auto* context = reinterpret_cast<getStringInstanceCMDContext*>(packet.payload);
    auto it = sqlDomains.find(context->domainId);
    if (it != sqlDomains.end()){
        size_t result = it->second->getString(context->key, reinterpret_cast<char*>(packet.outputBuffer), context->bufferSize);
        *reinterpret_cast<size_t*>(packet.outputBuffer) = result;
    }
}
void SQLFactory::existsCMD(FURCMDPacket& packet){
    auto* context = reinterpret_cast<existsInstanceCMDContext*>(packet.payload);
    auto it = sqlDomains.find(context->domainId);
    if (it != sqlDomains.end()){
        bool result = it->second->exists(context->key);
        *reinterpret_cast<bool*>(packet.outputBuffer) = result;
    }
}
void SQLFactory::closeCMD(FURCMDPacket& packet){
    auto* context = reinterpret_cast<closeInstanceCMDContext*>(packet.payload);
    auto it = sqlDomains.find(context->domainId);
    if (it != sqlDomains.end()){
        it->second->close();
    }
}
void SQLFactory::isOpenCMD(FURCMDPacket& packet){
    auto* context = reinterpret_cast<isOpenInstanceCMDContext*>(packet.payload);
    auto it = sqlDomains.find(context->domainId);
    if (it != sqlDomains.end()){
        bool result = it->second->isOpen();
        *reinterpret_cast<bool*>(packet.outputBuffer) = result;
    }
}