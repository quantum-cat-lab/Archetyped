#pragma once
#include "VFS.hpp"
#include "FURCMD/FURCMD.h"
#include <cstdint>
#include <string>

struct vfsResolveCMDContext {
    const char* virtualPath;
};

struct vfsSetRootCMDContext {
    char path[512];
};

struct vfsMountCMDContext {
    char scheme[64];
    char root[VFS::MAX_PATH_LENGTH];
    int32_t priority;
    uint8_t writable;
};

struct vfsUnmountCMDContext {
    char scheme[64];
};

class VFSDomains {
public:
    VFSDomains();
    ~VFSDomains() = default;
    
    static void resolvePathCMD(FURCMDPacket& packet);
    static void setContainerRootCMD(FURCMDPacket& packet);
    static void getContainerRootCMD(FURCMDPacket& packet);
    static void setUserRootCMD(FURCMDPacket& packet);
    static void mountCMD(FURCMDPacket& packet);
    static void unmountCMD(FURCMDPacket& packet);
    static void watchDrainCMD(FURCMDPacket& packet);
    };

// VFS watch drain — drains queued events (was used for hot-reload callbacks)