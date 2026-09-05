#pragma once

#include <string>
#include <cstring>
#include <filesystem>

/**
 * LocalVFS — in-process Virtual File System helper.
 *
 * Unlike the SDK VFS (which sends FURCMD packets to the engine kernel),
 * LocalVFS resolves virtual paths entirely in the calling module's address
 * space using pre-configured mount roots.  No packet dispatch, no fences,
 * no synchronization delays.
 *
 * Usage pattern — module.cpp (exactly once per module):
 *
 *   #include <SDK/archetyped/LocalVFS.hpp>
 *   char LocalVFS::s_containerRoot[LocalVFS::MAX_PATH] = {0};
 *   char LocalVFS::s_userRoot[LocalVFS::MAX_PATH] = {0};
 *
 *   void ModuleMain(IKernel* kernel, ModuleConfig config) {
 *       LocalVFS::SetContainerRoot(config.containerRoot);
 *       ...
 *   }
 *
 * Supported URI schemes:
 *   mod://   ->  <containerRoot>/<path-after-mod://>
 *   core://  ->  <containerRoot>/core/<path-after-core://>
 *   user://  ->  <userRoot>/<path-after-user://>
 *   other    ->  returned as-is (absolute / relative filesystem path)
 */
class LocalVFS {
public:
    static constexpr std::size_t MAX_PATH = 1024;

private:
    static char s_containerRoot[MAX_PATH];
    static char s_userRoot[MAX_PATH];

public:
    static void SetContainerRoot(const std::string& path) {
        std::strncpy(s_containerRoot, path.c_str(), MAX_PATH - 1);
        s_containerRoot[MAX_PATH - 1] = '\0';
    }

    static void SetUserRoot(const std::string& path) {
        std::strncpy(s_userRoot, path.c_str(), MAX_PATH - 1);
        s_userRoot[MAX_PATH - 1] = '\0';
    }

    // Mirrors VFS::IsPathPortionSafe: rejects empty/absolute portions,
    // backslashes, and ".." segments. Kept separate so LocalVFS has no
    // engine dependency; only base schemes are honored here.
    static bool IsPathPortionSafe(const std::string& virtualPath, size_t schemeLen) {
        const char* p = virtualPath.c_str() + schemeLen;
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

    static std::string ResolvePath(const std::string& virtualPath) {
        if (virtualPath.empty()) return "";

        if (virtualPath.find("mod://") == 0) {
            if (!IsPathPortionSafe(virtualPath, 6)) return "";
            std::string result = s_containerRoot;
            result += virtualPath.substr(6);  // skip "mod://"
            return result;
        }
        if (virtualPath.find("core://") == 0) {
            if (!IsPathPortionSafe(virtualPath, 7)) return "";
            std::string result = s_containerRoot;
            result += "core/";
            result += virtualPath.substr(7);  // skip "core://"
            return result;
        }
        if (virtualPath.find("user://") == 0) {
            if (!IsPathPortionSafe(virtualPath, 7)) return "";
            std::string result = s_userRoot;
            if (!result.empty() && result.back() != '/') result += '/';
            result += virtualPath.substr(7);  // skip "user://"
            return result;
        }

        return virtualPath;
    }

    static bool FileExists(const std::string& virtualPath) {
        return std::filesystem::exists(ResolvePath(virtualPath));
    }
};
