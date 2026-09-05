#pragma once

#include <cstdint>
#include <vector>

struct KernelCMDPacket
{
    uint8_t cmdId;
    void *in;
    void *out;
};

using KernelCMD = void (*) (KernelCMDPacket&) noexcept;

class KernelCMDManager {
public:
    KernelCMDManager() {
        table.reserve(UINT8_MAX);
    };
    ~KernelCMDManager() = default;
    void kernel_call(KernelCMDPacket& packet) noexcept;
    void kernel_register(KernelCMD cmd, uint8_t cmdiD);
private:

    std::vector<KernelCMD> table;

};
