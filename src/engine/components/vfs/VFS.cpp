#include "VFS.hpp"
#include <cstring>
#include <cstdint>
#include <sys/stat.h>

char VFS::s_containerRoot[VFS::MAX_PATH_LENGTH] = {0};
char VFS::s_userRoot[VFS::MAX_PATH_LENGTH] = {0};
VFS::MountPoint VFS::s_mounts[VFS::MAX_MOUNTS] = {};
uint32_t VFS::s_mountCount = 0;

bool VFS::StartsWith(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    size_t prefixLen = std::strlen(prefix);
    return std::strncmp(str, prefix, prefixLen) == 0;
}

// Path-portion safety check (zero-alloc).
// Rejects: empty portion, leading '/' or '\\' (absolute-path injection),
// any '\\' anywhere (Windows-style separator tricks), and ".." segments
// (traversal). Allows "." and empty trailing segments.
bool VFS::IsPathPortionSafe(const char* p) {
    if (!p) return false;
    if (p[0] == '\0' || p[0] == '/' || p[0] == '\\') return false;

    const char* segStart = p;
    for (const char* c = p; ; ++c) {
        if (*c == '\\') return false;
        if (*c == '/' || *c == '\0') {
            size_t len = static_cast<size_t>(c - segStart);
            if (len == 2 && segStart[0] == '.' && segStart[1] == '.') return false;
            if (*c == '\0') break;
            segStart = c + 1;
        }
    }
    return true;
}

void VFS::SafeAppend(char* dest, const char* src, size_t maxLen) {
    if (!dest || !src) return;
    size_t currentLen = std::strlen(dest);
    if (currentLen >= maxLen) return;

    size_t srcLen = std::strlen(src);
    size_t spaceLeft = maxLen - currentLen - 1;
    size_t toCopy = (srcLen < spaceLeft) ? srcLen : spaceLeft;
    
    std::memcpy(dest + currentLen, src, toCopy);
    dest[currentLen + toCopy] = '\0';
}

bool VFS::ParseScheme(const char* virtualPath, char* schemeBuf, size_t schemeBufSize, const char** outPathAfterPrefix) {
    if (!virtualPath || !schemeBuf || schemeBufSize == 0) return false;

    const char* sep = std::strstr(virtualPath, "://");
    if (!sep) return false;

    size_t schemeLen = static_cast<size_t>(sep - virtualPath);
    if (schemeLen == 0 || schemeLen >= schemeBufSize) return false;

    std::memcpy(schemeBuf, virtualPath, schemeLen);
    schemeBuf[schemeLen] = '\0';

    const char* pathAfterPrefix = sep + 3;
    if (!IsPathPortionSafe(pathAfterPrefix)) return false;

    if (outPathAfterPrefix) *outPathAfterPrefix = pathAfterPrefix;
    return true;
}

bool VFS::ResolveAgainst(const MountPoint& mp, const char* pathAfterPrefix, char* outPath, size_t bufferSize) {
    if (!outPath || bufferSize == 0) return false;

    std::strncpy(outPath, mp.root, bufferSize - 1);
    outPath[bufferSize - 1] = '\0';
    size_t len = std::strlen(outPath);
    if (len > 0 && outPath[len-1] != '/') {
        if (len < bufferSize - 1) outPath[len] = '/';
    }

    SafeAppend(outPath, pathAfterPrefix, bufferSize);
    return std::strlen(outPath) < bufferSize;
}

bool VFS::Resolve(const char* virtualPath, char* outPath, size_t bufferSize) {
    if (!outPath || bufferSize == 0) return false;
    outPath[0] = '\0';

    char scheme[64];
    const char* pathAfterPrefix = nullptr;
    if (!ParseScheme(virtualPath, scheme, sizeof(scheme), &pathAfterPrefix))
        return false;

    const MountPoint* best = nullptr;
    for (uint32_t i = 0; i < s_mountCount; ++i) {
        if (!s_mounts[i].used) continue;
        if (std::strcmp(s_mounts[i].scheme, scheme) != 0) continue;
        if (!best || s_mounts[i].priority > best->priority)
            best = &s_mounts[i];
    }
    if (!best) return false;

    return ResolveAgainst(*best, pathAfterPrefix, outPath, bufferSize);
}

bool VFS::ResolveFirstExisting(const char* virtualPath, char* outPath, size_t bufferSize) {
    if (!outPath || bufferSize == 0) return false;
    outPath[0] = '\0';

    char scheme[64];
    const char* pathAfterPrefix = nullptr;
    if (!ParseScheme(virtualPath, scheme, sizeof(scheme), &pathAfterPrefix))
        return false;

    // Iterate mounts of the scheme in priority order (descending);
    // return the first path that exists on disk. Override layers shadow
    // lower-priority ones with the same file.
    uint32_t bestIndex = UINT32_MAX;
    int bestPriority = INT32_MIN;
    for (uint32_t i = 0; i < s_mountCount; ++i) {
        if (!s_mounts[i].used) continue;
        if (std::strcmp(s_mounts[i].scheme, scheme) != 0) continue;

        char candidate[VFS::MAX_PATH_LENGTH];
        if (!ResolveAgainst(s_mounts[i], pathAfterPrefix, candidate, sizeof(candidate)))
            continue;

        struct stat st;
        if (stat(candidate, &st) == 0) {
            if (bestIndex == UINT32_MAX || s_mounts[i].priority > bestPriority) {
                bestIndex = i;
                bestPriority = s_mounts[i].priority;
            }
        }
    }

    if (bestIndex == UINT32_MAX) return false;
    return ResolveAgainst(s_mounts[bestIndex], pathAfterPrefix, outPath, bufferSize);
}

bool VFS::ResolveWritable(const char* virtualPath, char* outPath, size_t bufferSize) {
    if (!outPath || bufferSize == 0) return false;
    outPath[0] = '\0';

    char scheme[64];
    const char* pathAfterPrefix = nullptr;
    if (!ParseScheme(virtualPath, scheme, sizeof(scheme), &pathAfterPrefix))
        return false;

    const MountPoint* best = nullptr;
    for (uint32_t i = 0; i < s_mountCount; ++i) {
        if (!s_mounts[i].used || !s_mounts[i].writable) continue;
        if (std::strcmp(s_mounts[i].scheme, scheme) != 0) continue;
        if (!best || s_mounts[i].priority > best->priority)
            best = &s_mounts[i];
    }
    if (!best) return false;

    return ResolveAgainst(*best, pathAfterPrefix, outPath, bufferSize);
}

void VFS::SetContainerRoot(const char* rootPath) {
    if (!rootPath) return;
    std::strncpy(s_containerRoot, rootPath, MAX_PATH_LENGTH - 1);
    s_containerRoot[MAX_PATH_LENGTH - 1] = '\0';

    Mount("mod", s_containerRoot, 0, false);

    char coreRoot[MAX_PATH_LENGTH];
    std::strncpy(coreRoot, s_containerRoot, MAX_PATH_LENGTH - 1);
    coreRoot[MAX_PATH_LENGTH - 1] = '\0';
    size_t len = std::strlen(coreRoot);
    if (len > 0 && coreRoot[len-1] != '/') {
        if (len < MAX_PATH_LENGTH - 1) coreRoot[len] = '/';
    }
    SafeAppend(coreRoot, "core/", MAX_PATH_LENGTH);
    Mount("core", coreRoot, 0, false);
}

void VFS::GetContainerRoot(char* outBuffer, size_t bufferSize) {
    if (!outBuffer || bufferSize == 0) return;
    std::strncpy(outBuffer, s_containerRoot, bufferSize - 1);
    outBuffer[bufferSize - 1] = '\0';
}

void VFS::SetUserRoot(const char* userPath) {
    if (!userPath) return;
    std::strncpy(s_userRoot, userPath, MAX_PATH_LENGTH - 1);
    s_userRoot[MAX_PATH_LENGTH - 1] = '\0';

    Mount("user", s_userRoot, 0, true);
}

bool VFS::Mount(const char* scheme, const char* rootPath, int priority, bool writable) {
    if (!scheme || !rootPath || scheme[0] == '\0') return false;
    if (std::strlen(scheme) >= sizeof(MountPoint::scheme)) return false;

    for (uint32_t i = 0; i < s_mountCount; ++i) {
        if (!s_mounts[i].used) continue;
        if (std::strcmp(s_mounts[i].scheme, scheme) == 0 && s_mounts[i].priority == priority) {
            std::strncpy(s_mounts[i].root, rootPath, MAX_PATH_LENGTH - 1);
            s_mounts[i].root[MAX_PATH_LENGTH - 1] = '\0';
            s_mounts[i].writable = writable;
            return true;
        }
    }

    if (s_mountCount >= MAX_MOUNTS) return false;

    MountPoint& mp = s_mounts[s_mountCount++];
    std::strncpy(mp.scheme, scheme, sizeof(mp.scheme) - 1);
    mp.scheme[sizeof(mp.scheme) - 1] = '\0';
    std::strncpy(mp.root, rootPath, MAX_PATH_LENGTH - 1);
    mp.root[MAX_PATH_LENGTH - 1] = '\0';
    mp.priority = priority;
    mp.writable = writable;
    mp.used = true;
    return true;
}

bool VFS::Unmount(const char* scheme) {
    if (!scheme) return false;
    bool found = false;
    for (uint32_t i = 0; i < s_mountCount; ++i) {
        if (s_mounts[i].used && std::strcmp(s_mounts[i].scheme, scheme) == 0) {
            s_mounts[i].used = false;
            found = true;
        }
    }
    return found;
}

uint32_t VFS::GetMounts(MountPoint* out, uint32_t maxCount) {
    if (!out || maxCount == 0) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < s_mountCount && n < maxCount; ++i) {
        if (!s_mounts[i].used) continue;
        out[n++] = s_mounts[i];
    }
    return n;
}
