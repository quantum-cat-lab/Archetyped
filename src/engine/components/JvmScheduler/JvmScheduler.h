#pragma once

#include "../workScheduler/TaskQueue.h"
#include "../JvmHost/JvmHost.h"
#include <functional>


struct SchedConfig {
    int minWorkers = 2;
    int maxWorkers = 24;
    int growThreshold = 2;
    int shrinkThreshold = 3;
    int idleCheckMs = 15'000;
};

struct JvmTask { std::function<void(JNIEnv*)> fn; };

class JvmScheduler {
public:
    static JvmScheduler& instance();

    void stop();

    bool init();

    bool isRunning() const { return run.load(); }

    void submit(JvmTask task);
    void submitAndWait(JvmTask task);

private:
    JvmScheduler() = default;
    ~JvmScheduler() { stop(); }
    JvmScheduler(const JvmScheduler&) = delete;
    JvmScheduler& operator=(const JvmScheduler&) = delete;

    void workerLoop();
    void addWorker();
    bool shouldShrink();

    TaskQueue<JvmTask> taskQueue;
    std::mutex waitMutex;
    std::condition_variable condition;
    std::atomic<bool> run;
    std::vector<std::thread> workers;

    std::atomic<int> workerCount{0};
    std::atomic<int> busy{0};
    std::atomic<int> pending{0};
    JvmHost& host_ = JvmHost::instance();

    SchedConfig cfg_;

};
