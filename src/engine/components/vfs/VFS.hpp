#pragma once
#include <cstddef>
#include <cstdint>

/**
 * VFS (Virtual File System) - Core Component.
 * Responsible for resolving virtual paths (mod://, core://, user://) into absolute OS paths.
 * 
 * Design goals:
 * - Zero allocations in Core.
 * - Absolute ABI stability via const char* and buffer-passing.
 * - No dependency on std::string in Core.
 */
class VFS {
public:
    static constexpr size_t MAX_PATH_LENGTH = 1024;
    static constexpr size_t MAX_MOUNTS = 16;

    // One virtual-scheme mount. Multiple entries may share a scheme with
    // different priorities — higher priority wins for reads (override layers).
    struct MountPoint {
        char scheme[64];
        char root[VFS::MAX_PATH_LENGTH];
        int priority;    // higher = resolved first
        bool writable;   // writes allowed into this mount
        bool used;
    };
 
    /**
     * Resolves a virtual path to an absolute system path.
     * Resolves against the highest-priority mount for the scheme.
     *
     * @param outPath      Buffer where the absolute path will be written.
     * @param bufferSize   Size of the provided buffer.
     * @return true if resolved successfully, false otherwise.
     */
    static bool Resolve(const char* virtualPath, char* outPath, size_t bufferSize);

    /**
     * Resolves a virtual path against every mount of the scheme in priority
     * order and returns the first path that exists on disk (override layers).
     * Returns false if no mount produces an existing file.
     */
    static bool ResolveFirstExisting(const char* virtualPath, char* outPath, size_t bufferSize);

    /**
     * Resolves a virtual path against the highest-priority writable mount of
     * the scheme (write target). Returns false if the scheme has no writable
     * mount. Used for write/remove — read-only layers are never write targets.
     */
    static bool ResolveWritable(const char* virtualPath, char* outPath, size_t bufferSize);

    /**
     * Sets the root path for the currently active container.
     * Updates the base "mod://" and "core://" mounts (priority 0, read-only).
     */
    static void SetContainerRoot(const char* rootPath);

    static void GetContainerRoot(char* outBuffer, size_t bufferSize);

    /**
     * Sets the root path for user data (AppData, ~/.config, etc.).
     * Updates the base "user://" mount (priority 0, writable).
     */
    static void SetUserRoot(const char* userPath);

    /**
     * Mounts (or replaces) a virtual scheme pointing at rootPath.
     * Higher priority wins for reads; existing files in higher-priority
     * layers shadow lower ones.
     */
    static bool Mount(const char* scheme, const char* rootPath, int priority, bool writable);

    /** Removes all mounts for the scheme. */
    static bool Unmount(const char* scheme);

    /** Copies used mount points into out (up to maxCount). Returns count. */
    static uint32_t GetMounts(MountPoint* out, uint32_t maxCount);

    /**
     * Zero-alloc path-portion safety check used by all resolvers.
     * Rejects empty/absolute portions, backslashes, and ".." segments.
     * Public so higher layers (glob/find, LocalVFS) reuse the same rule.
     */
    static bool IsPathPortionSafe(const char* path);

private:
    static bool StartsWith(const char* str, const char* prefix);

    static bool ParseScheme(const char* virtualPath, char* schemeBuf, size_t schemeBufSize, const char** outPathAfterPrefix);

    static bool ResolveAgainst(const MountPoint& mp, const char* pathAfterPrefix, char* outPath, size_t bufferSize);
    
    static void SafeAppend(char* dest, const char* src, size_t maxLen);

    static char s_containerRoot[MAX_PATH_LENGTH];
    static char s_userRoot[MAX_PATH_LENGTH];
    static MountPoint s_mounts[MAX_MOUNTS];
    static uint32_t s_mountCount;
};