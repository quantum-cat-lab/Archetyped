#pragma once
#include <functional>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include "../workScheduler/workScheduler.h"

struct ScheduledTask {
    std::function<void()> fn;
    std::chrono::steady_clock::time_point nextExecution;
    std::chrono::milliseconds interval;
    bool cyclic;
    bool isTick;
    bool removed = false;
};

/**
 * SmartScheduler (Smart Scheduler) - Core Component.
 * Manages the scheduling of cyclic, delayed, and tick-based tasks.
 */
class SmartScheduler {
public:
    /**
     * Retrieves the singleton instance of the SmartScheduler.
     * 
     * @return Reference to the SmartScheduler instance.
     */
    static SmartScheduler& instance() {
        static SmartScheduler inst;
        return inst;
    }

    /**
     * Schedules a task to be executed cyclically at a fixed interval.
     * 
     * @param fn The function to execute.
     * @param intervalMs The interval between executions in milliseconds.
     * @return void
     */
    void scheduleCyclic(std::function<void()> fn, uint32_t intervalMs) {
        std::lock_guard<std::mutex> lock(tasksMutex);
        ScheduledTask task;
        task.fn = fn;
        task.interval = std::chrono::milliseconds(intervalMs);
        task.nextExecution = std::chrono::steady_clock::now() + task.interval;
        task.cyclic = true;
        task.isTick = false;
        tasks.push_back(std::move(task));
    }

    /**
     * Schedules a task to be executed once after a specified delay.
     * 
     * @param fn The function to execute.
     * @param delayMs The delay before execution in milliseconds.
     * @return void
     */
    void scheduleDelayed(std::function<void()> fn, uint32_t delayMs) {
        std::lock_guard<std::mutex> lock(tasksMutex);
        ScheduledTask task;
        task.fn = fn;
        task.interval = std::chrono::milliseconds(0);
        task.nextExecution = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
        task.cyclic = false;
        task.isTick = false;
        tasks.push_back(std::move(task));
    }

    /**
     * Schedules a task to be executed on every scheduler tick.
     * 
     * @param fn The function to execute.
     * @return void
     */
    void scheduleTick(std::function<void()> fn) {
        std::lock_guard<std::mutex> lock(tasksMutex);
        ScheduledTask task;
        task.fn = fn;
        task.isTick = true;
        task.cyclic = true;
        tasks.push_back(std::move(task));
    }

    /**
     * Starts the scheduler thread.
     * 
     * @return void
     */
    void run() {
        running = true;
        schedulerThread = std::thread(&SmartScheduler::update, this);
    }

    /**
     * Stops the scheduler and joins the scheduler thread.
     * 
     * @return void
     */
    void stop() {
        running = false;
        if (schedulerThread.joinable()) schedulerThread.join();
    }

private:
    SmartScheduler() : running(false) {}
    ~SmartScheduler() { stop(); }

    /**
     * Internal update loop that checks and dispatches scheduled tasks.
     * 
     * @return void
     */
    void update() {
        while (running) {
            auto now = std::chrono::steady_clock::now();
            std::vector<std::function<void()>> toExecute;

            {
                std::lock_guard<std::mutex> lock(tasksMutex);
                for (auto& task : tasks) {
                    if (task.removed) continue;
                    if (task.isTick || now >= task.nextExecution) {
                        toExecute.push_back(task.fn);
                        if (task.cyclic) {
                            task.nextExecution = now + task.interval;
                        } else {
                            task.removed = true;
                        }
                    }
                }
                tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [](const ScheduledTask& t) { return t.removed; }), tasks.end());
            }

            for (auto& fn : toExecute) {
                auto* ctx = new std::function<void()>(fn);
                ComputeTask ct;
                ct.fn = [](void* c) {
                    auto* f = static_cast<std::function<void()>*>(c);
                    (*f)();
                    delete f;
                };
                ct.context = ctx;
                
                if (ComputeInstance::get()) {
                    ComputeInstance::get()->scheduleTask(ct);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    std::vector<ScheduledTask> tasks;
    std::mutex tasksMutex;
    std::thread schedulerThread;
    std::atomic<bool> running;
};
