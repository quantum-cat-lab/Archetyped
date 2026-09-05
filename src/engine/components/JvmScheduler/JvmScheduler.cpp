#include "JvmScheduler.h"

#include <cstdio>

bool JvmScheduler::init() {
    if (run.load()) return true;
    if (!host_.isRunning() && !host_.init()) {
        std::fprintf(stderr, "[JvmScheduler] JvmHost not running, cannot init\n");
        return false;
    }
    run.store(true);
    for (int i = 0; i < cfg_.minWorkers; ++i) addWorker();
    std::fprintf(stderr, "[JvmScheduler] started workers=%d (min=%d max=%d)\n",
                 cfg_.minWorkers, cfg_.minWorkers, cfg_.maxWorkers);
    return true;
}

void JvmScheduler::stop() {
    if (!run.exchange(false)) return;
    condition.notify_all();
    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
    workers.clear();
    workerCount.store(0);
    busy.store(0);
    pending.store(0);
    std::fprintf(stderr, "[JvmScheduler] stopped\n");
}

void JvmScheduler::addWorker() {
    workerCount.fetch_add(1);
    workers.emplace_back(&JvmScheduler::workerLoop, this);
}

bool JvmScheduler::shouldShrink() {
    const int live = workerCount.load();
    const int b = busy.load();
    if (live > cfg_.minWorkers && (live - b) * cfg_.shrinkThreshold > live) {
        workerCount.store(live - 1);
        return true;
    }
    return false;
}

void JvmScheduler::workerLoop() {
    JNIEnv* env = host_.attachThread();
    if (!env) {
        workerCount.fetch_sub(1);
        return;
    }

    while (run.load()) {
        auto opt = taskQueue.pop();
        if (!opt) {
            std::unique_lock<std::mutex> lk(waitMutex);
            condition.wait_for(lk, std::chrono::milliseconds(cfg_.idleCheckMs),
                               [&] { return !taskQueue.empty() || !run.load(); });
            if (!run.load() && taskQueue.empty()) {
                workerCount.fetch_sub(1);
                host_.detachThread();
                return;
            }
            if (taskQueue.empty()) {
                if (shouldShrink()) {
                    workerCount.fetch_sub(1);
                    host_.detachThread();
                    return;
                }
                continue;
            }
            opt = taskQueue.pop();
            if (!opt) continue;
        }
        pending.fetch_sub(1);
        busy.fetch_add(1);
        (*opt).fn(env);
        busy.fetch_sub(1);
        if (env->ExceptionCheck()) JvmHost::hasException(env);
    }

    workerCount.fetch_sub(1);
    host_.detachThread();
}

void JvmScheduler::submit(JvmTask task) {
    if (!run.load()) return;
    taskQueue.push(task);
    pending.fetch_add(1);
    if (workerCount.load() * cfg_.growThreshold < pending.load() &&
        workerCount.load() < cfg_.maxWorkers) {
        addWorker();
    }
    condition.notify_one();
}

void JvmScheduler::submitAndWait(JvmTask task) {
    if (!run.load()) {
        JNIEnv* e = host_.attachThread();
        task.fn(e);
        if (e && e->ExceptionCheck()) JvmHost::hasException(e);
        host_.detachThread();
        return;
    }
    std::mutex doneM;
    std::condition_variable doneCv;
    bool done = false;
    submit(JvmTask{[&](JNIEnv* env) {
        task.fn(env);
        {
            std::lock_guard<std::mutex> lk(doneM);
            done = true;
        }
        doneCv.notify_one();
    }});
    std::unique_lock<std::mutex> lk(doneM);
    doneCv.wait(lk, [&] { return done; });
}
