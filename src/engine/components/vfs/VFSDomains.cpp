#include "VFSDomains.h"
#include "VFSWatch.hpp"
#include "core/FractalKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstdio>
#include <iostream>


constexpr uint32_t vfsResolveHash = fnv1aHashConst("archetyped:vfs:resolve");
constexpr uint32_t vfsSetContainerRootHash = fnv1aHashConst("archetyped:vfs:setContainerRoot");
constexpr uint32_t vfsGetContainerRootHash = fnv1aHashConst("archetyped:vfs:getContainerRoot");
constexpr uint32_t vfsSetUserRootHash = fnv1aHashConst("archetyped:vfs:setUserRoot");
constexpr uint32_t vfsMountHash = fnv1aHashConst("archetyped:vfs:mount");
constexpr uint32_t vfsUnmountHash = fnv1aHashConst("archetyped:vfs:unmount");
constexpr uint32_t vfsWatchDrainHash = fnv1aHashConst("archetyped:vfs:watchDrain");

VFSDomains::VFSDomains() {
    FractalKernel::instance().registerCMDMethod(vfsResolveHash, &VFSDomains::resolvePathCMD);
    FractalKernel::instance().registerCMDMethod(vfsSetContainerRootHash, &VFSDomains::setContainerRootCMD);
    FractalKernel::instance().registerCMDMethod(vfsGetContainerRootHash, &VFSDomains::getContainerRootCMD);
    FractalKernel::instance().registerCMDMethod(vfsSetUserRootHash, &VFSDomains::setUserRootCMD);
    FractalKernel::instance().registerCMDMethod(vfsMountHash, &VFSDomains::mountCMD);
    FractalKernel::instance().registerCMDMethod(vfsUnmountHash, &VFSDomains::unmountCMD);
    FractalKernel::instance().registerCMDMethod(vfsWatchDrainHash, &VFSDomains::watchDrainCMD);
}


void VFSDomains::getContainerRootCMD(FURCMDPacket& packet) {
    if (!packet.outputBuffer) return;
    VFS::GetContainerRoot(static_cast<char*>(packet.outputBuffer), VFS::MAX_PATH_LENGTH);
}

void VFSDomains::resolvePathCMD(FURCMDPacket& packet) {
    std::cout << "[VFS Service] Processing resolvePathCMD..." << std::endl;
    auto* ctx = reinterpret_cast<vfsResolveCMDContext*>(packet.payload);
    char* outBuffer = static_cast<char*>(packet.outputBuffer);
    
    if (!ctx || !outBuffer) {
        std::cerr << "[VFS Service] Error: missing context or buffer!" << std::endl;
        return;
    }

    if (VFS::Resolve(ctx->virtualPath, outBuffer, VFS::MAX_PATH_LENGTH)) {
        std::cout << "[VFS Service] Resolved " << ctx->virtualPath << " -> " << outBuffer << std::endl;
    } else {
        std::cerr << "[VFS Service] Failed to resolve: " << ctx->virtualPath << std::endl;
        outBuffer[0] = '\0';
    }
}

void VFSDomains::setContainerRootCMD(FURCMDPacket& packet) {
    auto* ctx = reinterpret_cast<vfsSetRootCMDContext*>(packet.payload);
    if (!ctx) return;
    VFS::SetContainerRoot(ctx->path);
}

void VFSDomains::setUserRootCMD(FURCMDPacket& packet) {
    auto* ctx = reinterpret_cast<vfsSetRootCMDContext*>(packet.payload);
    if (!ctx) return;
    VFS::SetUserRoot(ctx->path);
}

void VFSDomains::mountCMD(FURCMDPacket& packet) {
    auto* ctx = reinterpret_cast<vfsMountCMDContext*>(packet.payload);
    if (!ctx) return;
    VFS::Mount(ctx->scheme, ctx->root, ctx->priority, ctx->writable != 0);
}

void VFSDomains::unmountCMD(FURCMDPacket& packet) {
    auto* ctx = reinterpret_cast<vfsUnmountCMDContext*>(packet.payload);
    if (!ctx) return;
    VFS::Unmount(ctx->scheme);
}

void VFSDomains::watchDrainCMD(FURCMDPacket& packet) {
    // Lua removed: drain queued events to prevent unbounded queue growth
    (void)packet;
    VFSWatch::instance().drain();
}