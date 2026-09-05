#include "KernelCommands.h"

void KernelCMDManager::kernel_call(KernelCMDPacket &packet) noexcept {

    table[packet.cmdId](packet);

}

void KernelCMDManager::kernel_register(KernelCMD cmd, uint8_t cmdId) {

    table[cmdId] = cmd;

}
