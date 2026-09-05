#include "VFSWatch.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <poll.h>
#endif

VFSWatch& VFSWatch::instance() {
    static VFSWatch svc;
    return svc;
}

VFSWatch::VFSWatch() : m_running(true) {
#ifdef __linux__
    m_inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotifyFd < 0) {
        std::cerr << "[VFSWatch] inotify_init1 failed (" << errno
                  << "); falling back to mtime polling" << std::endl;
    }
#endif
    m_thread = std::thread(&VFSWatch::threadLoop, this);
}

VFSWatch::~VFSWatch() {
    m_running = false;
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();

#ifdef __linux__
    if (m_inotifyFd >= 0) ::close(m_inotifyFd);
#endif
}

const char* VFSWatch::eventTypeName(EventType t) const {
    switch (t) {
        case Modified: return "modified";
        case Created:  return "created";
        case Deleted:  return "deleted";
    }
    return "unknown";
}

int VFSWatch::watch(const std::string& absPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

#ifdef __linux__
    if (m_inotifyFd >= 0) {
        int wd = inotify_add_watch(m_inotifyFd, absPath.c_str(),
                                   IN_CLOSE_WRITE | IN_CREATE | IN_DELETE |
                                   IN_MOVED_TO | IN_MOVED_FROM);
        if (wd < 0) {
            std::cerr << "[VFSWatch] inotify_add_watch(" << absPath
                      << ") failed (" << errno << ")" << std::endl;
            return -1;
        }
        WatchEntry entry;
        entry.id = m_nextId++;
        entry.absPath = absPath;
        entry.wd = wd;
        m_watches.push_back(std::move(entry));
        return m_watches.back().id;
    }
#endif

    // mtime-poll fallback (non-Linux or inotify unavailable).
    WatchEntry entry;
    entry.id = m_nextId++;
    entry.absPath = absPath;
    struct stat st;
    if (stat(absPath.c_str(), &st) == 0) {
        entry.lastExists = true;
        entry.lastMtimeNs = static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL +
                            static_cast<long long>(st.st_mtim.tv_nsec);
        entry.lastSize = static_cast<long long>(st.st_size);
    }
    m_watches.push_back(std::move(entry));
    m_cv.notify_all(); // wake poller immediately so it sees the new entry
    return m_watches.back().id;
}

bool VFSWatch::unwatch(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < m_watches.size(); ++i) {
        if (m_watches[i].id != id) continue;
#ifdef __linux__
        if (m_inotifyFd >= 0 && m_watches[i].wd >= 0)
            inotify_rm_watch(m_inotifyFd, m_watches[i].wd);
#endif
        m_watches.erase(m_watches.begin() + static_cast<std::ptrdiff_t>(i));
        return true;
    }
    return false;
}

bool VFSWatch::hasWatches() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_watches.empty();
}

std::vector<VFSWatch::WatchEvent> VFSWatch::drain() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<WatchEvent> out;
    out.reserve(m_events.size());
    while (!m_events.empty()) {
        out.push_back(std::move(m_events.front()));
        m_events.pop_front();
    }
    return out;
}

void VFSWatch::enqueue(int id, EventType type, const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.push_back(WatchEvent{id, type, path});
}

void VFSWatch::threadLoop() {
#ifdef __linux__
    if (m_inotifyFd >= 0) {
        // inotify path: poll with timeout, translate wd -> id, enqueue.
        char buffer[4096];
        while (m_running) {
            std::vector<WatchEntry> snapshot;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                snapshot = m_watches;
            }

            struct pollfd pfd;
            pfd.fd = m_inotifyFd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            int pr = poll(&pfd, 1, 200);
            if (pr < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (pr == 0) continue; // timeout, loop (also serves as wake check)

            ssize_t n = ::read(m_inotifyFd, buffer, sizeof(buffer));
            if (n <= 0) continue;

            size_t off = 0;
            while (off < static_cast<size_t>(n)) {
                auto* ev = reinterpret_cast<struct inotify_event*>(buffer + off);
                off += sizeof(struct inotify_event) + ev->len;

                int id = -1;
                std::string absPath;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (const auto& e : m_watches) {
                        if (e.wd == ev->wd) { id = e.id; absPath = e.absPath; break; }
                    }
                }
                if (id < 0) continue;

                EventType type;
                if (ev->mask & (IN_DELETE | IN_MOVED_FROM)) type = Deleted;
                else if (ev->mask & (IN_CREATE | IN_MOVED_TO)) type = Created;
                else type = Modified;
                enqueue(id, type, absPath);
            }
        }
        return;
    }
#endif

    // mtime-poll fallback: stat every watch every 250ms, diff state.
    while (m_running) {
        std::vector<WatchEntry> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            snapshot = m_watches;
        }

        if (!snapshot.empty()) {
            for (auto& entry : snapshot) {
                struct stat st;
                bool exists = (stat(entry.absPath.c_str(), &st) == 0);
                long long mtimeNs = 0, size = 0;
                if (exists) {
                    mtimeNs = static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL +
                              static_cast<long long>(st.st_mtim.tv_nsec);
                    size = static_cast<long long>(st.st_size);
                }

                if (exists && !entry.lastExists)
                    enqueue(entry.id, Created, entry.absPath);
                else if (!exists && entry.lastExists)
                    enqueue(entry.id, Deleted, entry.absPath);
                else if (exists && (mtimeNs != entry.lastMtimeNs || size != entry.lastSize))
                    enqueue(entry.id, Modified, entry.absPath);

                entry.lastExists = exists;
                entry.lastMtimeNs = mtimeNs;
                entry.lastSize = size;
            }
            // Publish updated state back into m_watches (vector copy is fine
            // here — the poll thread is the only writer besides watch()).
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_watches.size() == snapshot.size()) m_watches = std::move(snapshot);
            }
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(250),
                      [this] { return !m_running; });
    }
}
