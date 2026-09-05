#include "WSFactory.h"
#include "core/FractalKernel.h"
constexpr uint32_t registerWSDomainHash = fnv1aHashConst("archetyped:work_scheduler:registerWSInstance");
constexpr uint32_t scheduleTaskHash = fnv1aHashConst("archetyped:work_scheduler:scheduleTask");
ankerl::unordered_dense::map<uint32_t, ComputeInstance*, IdentityHash> WSFactory::computeSchedulers;

WSFactory::WSFactory(){
    FractalKernel::instance().registerCMDMethod(registerWSDomainHash, &registerWSDomain);
    FractalKernel::instance().registerCMDMethod(scheduleTaskHash, &scheduleTaskCMD);
}
void WSFactory::registerWSDomain(FURCMDPacket& packet){
    uint32_t domainId = *reinterpret_cast<uint32_t*>(packet.payload);
    if (computeSchedulers.find(domainId) == computeSchedulers.end()){
        computeSchedulers[domainId] = new ComputeInstance();
    }
}
void WSFactory::scheduleTaskCMD(FURCMDPacket& packet){
    scheduleTaskInstanceCMDContext& context = *reinterpret_cast<scheduleTaskInstanceCMDContext*>(packet.payload);
    uint32_t domainId = context.domainId;

    if (computeSchedulers.find(domainId) != computeSchedulers.end()){
        ComputeTask task = context.task;
        computeSchedulers[domainId]->scheduleTask(task);
    }
}
