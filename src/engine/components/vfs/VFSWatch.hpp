#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef __linux__
#include <sys/inotify.h>
#endif

/**
 * VFSWatch — file watching service.
 *
 * One background thread owns the platform watch handle (inotify on Linux,
 * mtime polling elsewhere). Changes are pushed into a small thread-safe
 * queue; the engine drains it on the engine thread (via a frame-tick FURCMD)
 * so callbacks (kt/kts/cs) always run on the thread that owns the runtime state.
 *
 * Hot-reload use case: watch a path once, re-read the file from the
 * callback when "modified" arrives.
 */
class VFSWatch {
public:
    enum EventType {
        Modified = 0,
        Created = 1,
        Deleted = 2,
    };

    struct WatchEvent {
        int id;
        EventType type;
        std::string path;
    };

    static VFSWatch& instance();

    /** Registers an absolute path. Returns watch id (>=1) or -1 on failure. */
    int watch(const std::string& absPath);

    /** Removes a watch by id. Returns false if the id is unknown. */
    bool unwatch(int id);

    /** Moves all pending events out of the queue. Thread-safe. */
    std::vector<WatchEvent> drain();

    /** True when at least one path is registered. */
    bool hasWatches() const;

private:
    VFSWatch();
    ~VFSWatch();
    VFSWatch(const VFSWatch&) = delete;
    VFSWatch& operator=(const VFSWatch&) = delete;

    void threadLoop();
    void enqueue(int id, EventType type, const std::string& path);
    const char* eventTypeName(EventType t) const;

    std::thread m_thread;
    std::atomic<bool> m_running;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    struct WatchEntry {
        int id;
        std::string absPath;
#ifdef __linux__
        int wd; // inotify watch descriptor
#endif
        // mtime fallback state
        bool lastExists = false;
        long long lastMtimeNs = 0;
        long long lastSize = 0;
    };

    std::vector<WatchEntry> m_watches;
    std::deque<WatchEvent> m_events;
    int m_nextId = 1;

#ifdef __linux__
    int m_inotifyFd = -1;
#endif
};
