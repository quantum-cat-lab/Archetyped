#pragma once
#include "../SmartScheduler/SmartScheduler.h"
#include <vector>
#include <functional>

struct ECSSystem {
    std::function<void()> updateFn;
    uint32_t frequencyMs; // 0 for every tick
};

/**
 * SystemExecution (System Execution) - Core Component.
 * Responsible for registering and executing ECS systems via the SmartScheduler.
 */
class SystemExecution {
public:
    /**
     * Retrieves the singleton instance of SystemExecution.
     * 
     * @return Reference to the SystemExecution instance.
     */
    static SystemExecution& instance() {
        static SystemExecution inst;
        return inst;
    }

    /**
     * Registers a system update function to be executed at a specified frequency.
     * 
     * @param updateFn The function to be called for system updates.
     * @param frequencyMs The execution frequency in milliseconds (0 for every tick).
     * @return void
     */
    void registerSystem(std::function<void()> updateFn, uint32_t frequencyMs = 0) {
        if (frequencyMs == 0) {
            SmartScheduler::instance().scheduleTick(updateFn);
        } else {
            SmartScheduler::instance().scheduleCyclic(updateFn, frequencyMs);
        }
    }

private:
    SystemExecution() = default;
};
