#pragma once
#include "FURCMD/FURCMD.h"
#include "core/ISubmoduleHost.h"
#include "ankerl/unordered_dense.h"
#include "json.hpp"
#include <string>
#include <vector>
#include <memory>

using json = nlohmann::json;

/**
 * ContainerLoader (Container Loader) - Core Component.
 * Responsible for loading and managing active containers
 * with multi-stage loading (boot → hosts → submodules → finalize).
 */
class ContainerLoader {
public:
    ContainerLoader();
    ~ContainerLoader() = default;

private:
    struct HostEntry {
        std::string id;
        ISubmoduleHost* host;
    };

    ankerl::unordered_dense::map<uint32_t, std::unique_ptr<HostEntry>> m_hosts;

    void loadActiveContainer();

    static void reloadCMD(FURCMDPacket& packet);
    std::string getActiveContainerName();

    void loadStaged(const nlohmann::json& j, const std::string& contName,
                    const ModuleConfig& config);
    void loadLegacyFormat(const nlohmann::json& j, const std::string& contName,
                          const ModuleConfig& config);

    ISubmoduleHost* findHost(const std::string& hostId);
};
