#pragma once
#include "FractalSDK.h"
#include "IKernel.h"
#include <SDK/archetyped/hash/hash.h>
#include <string>
#include <cstring>
#include <thread>
#include <filesystem>

/**
 * VFS - Virtual File System SDK.
 * Provides a high-level interface for resolving virtual paths
 * into absolute system paths via the Fractal Engine kernel.
 */
class VFS {
public:
    /**
     * Resolves a virtual path (e.g., "mod://AmethystY/assets/mesh.obj")
     * to an absolute system path.
     * Returns an empty string if resolution fails or SDK is not initialized.
     */
    static std::string ResolvePath(const char* virtualPath) {
        if (!virtualPath) return "";
        
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return ""; 
        
        char resolved[1024];
        resolved[0] = '\0';
        
        vfsResolveCMDContext ctx{virtualPath};
        FURCMDPacket packet;
        packet.methodHash = vfsResolveHash;
        packet.payload = &ctx;
        packet.payloadSize = sizeof(vfsResolveCMDContext);
        packet.outputBuffer = resolved;
        
        Ticket* ticket = sdk->allocateTicket();
        packet.fence = reinterpret_cast<uint64_t*>(&ticket->fence);
        
        sdk->sendPacket(packet);
        
        while (!ticket->isReady()) {
            std::this_thread::yield();
        }
        
        return std::string(resolved);
    }

    static bool FileExists(const char* virtualPath) {
        std::string path = ResolvePath(virtualPath);
        if (path.empty()) return false;
        return std::filesystem::exists(path);
    }

    static void SetContainerRoot(const char* path) {
        if (!path) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;

        vfsSetRootCMDContext ctx{};
        std::strncpy(ctx.path, path, sizeof(ctx.path) - 1);
        ctx.path[sizeof(ctx.path) - 1] = '\0';
        FURCMDPacket packet;
        packet.methodHash = vfsSetContainerRootHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        sdk->sendPacket(packet);
    }

    static void SetUserRoot(const char* path) {
        if (!path) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;

        vfsSetRootCMDContext ctx{};
        std::strncpy(ctx.path, path, sizeof(ctx.path) - 1);
        ctx.path[sizeof(ctx.path) - 1] = '\0';
        FURCMDPacket packet;
        packet.methodHash = vfsSetUserRootHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        sdk->sendPacket(packet);
    }

    /**
     * Mounts a virtual scheme pointing at an absolute root path.
     * Higher priority wins for reads; writable allows writes through this mount.
     */
    static void Mount(const char* scheme, const char* rootPath, int priority = 0, bool writable = false) {
        if (!scheme || !rootPath) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;

        vfsMountCMDContext ctx{};
        std::strncpy(ctx.scheme, scheme, sizeof(ctx.scheme) - 1);
        std::strncpy(ctx.root, rootPath, sizeof(ctx.root) - 1);
        ctx.priority = priority;
        ctx.writable = writable ? 1 : 0;

        FURCMDPacket packet;
        packet.methodHash = vfsMountHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        sdk->sendPacket(packet);
    }

    /** Removes all mounts for the scheme. */
    static void Unmount(const char* scheme) {
        if (!scheme) return;
        auto* sdk = FractalSDK::SDK::Get();
        if (!sdk) return;

        vfsUnmountCMDContext ctx{};
        std::strncpy(ctx.scheme, scheme, sizeof(ctx.scheme) - 1);

        FURCMDPacket packet;
        packet.methodHash = vfsUnmountHash;
        packet.payloadSize = sizeof(ctx);
        packet.payload = &ctx;

        sdk->sendPacket(packet);
    }
};
