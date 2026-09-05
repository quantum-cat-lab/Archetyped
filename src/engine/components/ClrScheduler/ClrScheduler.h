#pragma once
#include "../workScheduler/TaskQueue.h"
#include "../ClrHost/ClrHost.h"
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

struct ClrTask { std::function<void()> fn; };

class ClrScheduler {
public:
    static ClrScheduler& instance();
    bool init();
    void stop();
    bool isRunning() const { return run.load(); }
    void submit(ClrTask task);
    void submitAndWait(ClrTask task);
private:
    ClrScheduler() = default;
    ~ClrScheduler() { stop(); }
    ClrScheduler(const ClrScheduler&) = delete;
    ClrScheduler& operator=(const ClrScheduler&) = delete;

    void workerLoop();
    void addWorker();
    bool shouldShrink();

    struct SchedConfig { int minWorkers=2; int maxWorkers=24; int growThreshold=2; int shrinkThreshold=3; int idleCheckMs=15000; } cfg_;

    TaskQueue<ClrTask> taskQueue;
    std::mutex waitMutex;
    std::condition_variable condition;
    std::atomic<bool> run{false};
    std::vector<std::thread> workers;
    std::atomic<int> workerCount{0};
    std::atomic<int> busy{0};
    std::atomic<int> pending{0};
    ClrHost& host_ = ClrHost::instance();
};
