#include "CmdContainer.h"
#include "../CLIUtils.h"
#include "../Workspace.h"
#include "../Process.h"

#include <fstream>
#include <iostream>

int CmdContainerInit::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);
    if (name.empty()) { std::cerr << "Error: Container name required.\n"; return 1; }

    try {
        fs::path cp = ws.containerPath(name);
        fs::create_directories(cp);
        json cj = {
            {"container_id", name}, {"name", name}, {"version", "1.0.0"},
            {"preload", json::array({"NexusXCore"})}, {"load_order", json::array()},
            {"metadata", {{"target_platform", "Linux"}, {"build_type", "Debug"}}},
            {"global_settings", {{"log_level", "Verbose"}}}
        };
        std::ofstream f(cp / "container.json");
        f << cj.dump(4);
        cli::printSuccess("Container '" + name + "' initialized");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
        return 0;
}

int CmdContainerInfo::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);
    if (name.empty()) { cli::printError("Container name required"); return 1; }

    json cj = ws.loadContainerJson(name);
    if (cj.empty() || !cj.contains("container_id")) {
        cli::printError("Container '" + name + "' not found");
        return 1;
    }

    cli::printSection(cli::iconContainer(), "Container: " + name);
    std::cout << "    " << cli::gray("ID:")      << "      " << cli::bold(cj.value("container_id", "N/A")) << "\n";
    std::cout << "    " << cli::gray("Name:")    << "    " << cli::bold(cj.value("name", "N/A")) << "\n";
    std::cout << "    " << cli::gray("Version:") << " " << cj.value("version", "N/A") << "\n";

    if (cj.contains("load_order") && cj["load_order"].is_array()) {
        std::string mods;
        for (auto& m : cj["load_order"]) {
            if (!mods.empty()) mods += ", ";
            mods += m.get<std::string>();
        }
        if (mods.empty()) mods = "None";
        std::cout << "    " << cli::gray("Mods:") << "   " << mods << "\n";
    }
    std::cout << "    " << cli::gray("Path:") << "   " << ws.containerPath(name).string() << "\n";
        return 0;
}

int CmdContainerAdd::execute(const Args& args, Workspace& ws) {
    if (args.positional.size() < 2) { std::cerr << "Error: Container and ModuleInstance names required.\n"; return 1; }
    std::string contName = args.pos(0);
    std::string modId    = args.pos(1);

    json cj = ws.loadContainerJson(contName);
    if (cj.empty()) return 1;

    std::vector<std::string> order = cj["load_order"].get<std::vector<std::string>>();
    if (std::find(order.begin(), order.end(), modId) == order.end()) {
        cj["load_order"].push_back(modId);
        ws.saveContainerJson(contName, cj);
        cli::printSuccess("ModuleInstance '" + modId + "' added to container '" + contName + "'");
    }
        return 0;
}

int CmdContainerRemove::execute(const Args& args, Workspace& ws) {
    if (args.positional.size() < 2) { std::cerr << "Error: Container and ModuleInstance names required.\n"; return 1; }
    std::string contName = args.pos(0);
    std::string modId    = args.pos(1);

    json cj = ws.loadContainerJson(contName);
    if (cj.empty()) return 1;

    std::vector<std::string> order = cj["load_order"].get<std::vector<std::string>>();
    auto it = std::find(order.begin(), order.end(), modId);
    if (it != order.end()) {
        order.erase(it);
        cj["load_order"] = order;
        ws.saveContainerJson(contName, cj);
        cli::printSuccess("ModuleInstance '" + modId + "' removed from container '" + contName + "'");
    }
        return 0;
}

int CmdContainerCp::execute(const Args& args, Workspace& ws) {
    if (args.positional.size() < 2) { std::cerr << "Error: Src and Dest required.\n"; return 1; }
    fs::path src = ws.containerPath(args.pos(0));
    fs::path dst = ws.containerPath(args.pos(1));
    fs::copy(src, dst, fs::copy_options::recursive);
    cli::printSuccess("Container cloned successfully");
        return 0;
}

int CmdContainerRm::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);
    if (name.empty()) { std::cerr << "Error: Name required.\n"; return 1; }
    fs::remove_all(ws.containerPath(name));
    cli::printSuccess("Container deleted");
        return 0;
}

int CmdContainerArc::execute(const Args& args, Workspace& ws) {
    std::string name = args.pos(0);
    if (name.empty()) { std::cerr << "Error: Name required.\n"; return 1; }
    std::string cmd = "tar -czvf " + name + ".tar.gz -C "
        + ws.containersRoot().string() + " " + name;
    Process::system(cmd);
        return 0;
}

int CmdContainerUnarc::execute(const Args& args, Workspace& ws) {
    fs::path arcPath(args.pos(0));
    if (arcPath.empty()) { std::cerr << "Error: Path required.\n"; return 1; }

    std::string filename = arcPath.filename().string();
    std::string contName = filename;
    size_t pos = filename.find(".tar.gz");
    if (pos != std::string::npos) contName = filename.substr(0, pos);

    std::string cmd = "tar -xzvf " + arcPath.string() + " -C " + ws.containersRoot().string();
    Process::system(cmd);
    std::cout << "Container '" << contName << "' unpacked.\n";
        return 0;
}
