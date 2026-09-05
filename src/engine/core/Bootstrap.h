#pragma once
#include <cstdint>
#include <memory>

class Bootstrap {
public:
    static Bootstrap& instance();
    void init();
    void shutdown();
    bool tick();
    bool shouldExit();
private:
    Bootstrap() = default;
    static constexpr uint32_t vfsWatchDrainHash = 0x8a3c9d1f;
    bool inited_ = false;
};
