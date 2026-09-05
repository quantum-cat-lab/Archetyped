#include "ClrScheduler.h"
#include <cstdio>

ClrScheduler& ClrScheduler::instance() { static ClrScheduler s; return s; }

bool ClrScheduler::init() {
    if (run.load()) return true;
    if (!host_.isRunning() && !host_.init()) {
        std::fprintf(stderr, "[ClrScheduler] ClrHost not running\n");
        return false;
    }
    run.store(true);
    for (int i=0;i<cfg_.minWorkers;++i) addWorker();
    std::fprintf(stderr, "[ClrScheduler] started workers=%d\n", cfg_.minWorkers);
    return true;
}
void ClrScheduler::stop() {
    if (!run.exchange(false)) return;
    condition.notify_all();
    for (auto& t: workers) if (t.joinable()) t.join();
    workers.clear();
    workerCount.store(0); busy.store(0); pending.store(0);
    std::fprintf(stderr, "[ClrScheduler] stopped\n");
}
void ClrScheduler::addWorker() { workerCount.fetch_add(1); workers.emplace_back(&ClrScheduler::workerLoop, this); }
bool ClrScheduler::shouldShrink() {
    int live=workerCount.load(), b=busy.load();
    if (live>cfg_.minWorkers && (live-b)*cfg_.shrinkThreshold>live) { workerCount.store(live-1); return true; }
    return false;
}
void ClrScheduler::workerLoop() {
    while (run.load()) {
        auto opt = taskQueue.pop();
        if (!opt) {
            std::unique_lock<std::mutex> lk(waitMutex);
            condition.wait_for(lk, std::chrono::milliseconds(cfg_.idleCheckMs), [&]{return !taskQueue.empty()||!run.load();});
            if (!run.load() && taskQueue.empty()) { workerCount.fetch_sub(1); return; }
            if (taskQueue.empty()) { if (shouldShrink()) { workerCount.fetch_sub(1); return; } continue; }
            opt=taskQueue.pop(); if(!opt) continue;
        }
        pending.fetch_sub(1); busy.fetch_add(1);
        try { (*opt).fn(); } catch(...) { std::fprintf(stderr,"[ClrScheduler] task exception\n"); }
        busy.fetch_sub(1);
    }
    workerCount.fetch_sub(1);
}
void ClrScheduler::submit(ClrTask task) {
    if (!run.load()) return;
    taskQueue.push(std::move(task)); pending.fetch_add(1);
    if (workerCount.load()*cfg_.growThreshold < pending.load() && workerCount.load() < cfg_.maxWorkers) addWorker();
    condition.notify_one();
}
void ClrScheduler::submitAndWait(ClrTask task) {
    if (!run.load()) { task.fn(); return; }
    std::mutex m; std::condition_variable cv; bool done=false;
    submit(ClrTask{[&]{ task.fn(); { std::lock_guard<std::mutex> lk(m); done=true; } cv.notify_one(); }});
    std::unique_lock<std::mutex> lk(m); cv.wait(lk, [&]{return done;});
}
