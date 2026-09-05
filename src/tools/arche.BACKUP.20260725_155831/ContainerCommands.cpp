#include "Arche.h"

// --- Init Container ---
void Arche::doInitCont(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "Error: Container name required.\n"; return; }
    std::string name = args[0];
    fs::path contPath = projectRoot_ / "container" / name;
    try {
        fs::create_directories(contPath);
        json contJson = {{"container_id", name}, {"name", name}, {"version", "1.0.0"}, {"preload", json::array({"NexusXCore"})}, {"load_order", json::array()}, {"metadata", {{"target_platform", "Linux"}, {"build_type", "Debug"}}}, {"global_settings", {{"log_level", "Verbose"}}}};
        std::ofstream file(contPath / "container.json");
        file << contJson.dump(4);
        cli::printSuccess("Container '" + name + "' initialized");
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << std::endl; }
}

// --- Info Container ---
void Arche::doInfoCont(const std::vector<std::string>& args) {
    if (args.empty()) { cli::printError("Container name required"); return; }
    fs::path jsonPath = projectRoot_ / "container" / args[0] / "container.json";
    if (!fs::exists(jsonPath)) { cli::printError("Container '" + args[0] + "' not found"); return; }
    try {
        std::ifstream file(jsonPath);
        json cJson; file >> cJson;
        std::vector<std::string> lines;
        lines.push_back(cli::gray("ID:") + "      " + cli::bold(cJson.value("container_id", "N/A")));
        lines.push_back(cli::gray("Name:") + "    " + cli::bold(cJson.value("name", "N/A")));
        lines.push_back(cli::gray("Version:") + " " + cJson.value("version", "N/A"));
        if (cJson.contains("load_order") && cJson["load_order"].is_array()) {
            std::string mods;
            for (auto& mod : cJson["load_order"]) {
                if (!mods.empty()) mods += ", ";
                mods += mod.get<std::string>();
            }
            if (mods.empty()) mods = "None";
            lines.push_back(cli::gray("Mods:") + "   " + mods);
        }
        lines.push_back(cli::gray("Path:") + "   " + (projectRoot_ / "container" / args[0]).string());
        cli::printSection(cli::iconContainer(), "Container: " + args[0]);
        for (auto& l : lines) std::cout << "    " << l << "\n";
    } catch (const std::exception& e) { cli::printError(e.what()); }
}

// --- Add ModuleInstance ---
void Arche::doAddMod(const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cerr << "Error: Container and ModuleInstance names required.\n"; return; }
    std::string contName = args[0];
    std::string modId = args[1];
    fs::path contJsonPath = projectRoot_ / "container" / contName / "container.json";
    if (!fs::exists(contJsonPath)) return;
    try {
        std::ifstream inFile(contJsonPath);
        json contJson; inFile >> contJson; inFile.close();
        std::vector<std::string> order = contJson["load_order"].get<std::vector<std::string>>();
        if (std::find(order.begin(), order.end(), modId) == order.end()) {
            contJson["load_order"].push_back(modId);
            std::ofstream outFile(contJsonPath);
            outFile << contJson.dump(4);
            cli::printSuccess("ModuleInstance '" + modId + "' added to container '" + contName + "'");
        }
    } catch (...) {}
}

// --- Remove ModuleInstance ---
void Arche::doRemMod(const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cerr << "Error: Container and ModuleInstance names required.\n"; return; }
    std::string contName = args[0];
    std::string modId = args[1];
    fs::path contJsonPath = projectRoot_ / "container" / contName / "container.json";
    if (!fs::exists(contJsonPath)) return;
    try {
        std::ifstream inFile(contJsonPath);
        json contJson; inFile >> contJson; inFile.close();
        std::vector<std::string> order = contJson["load_order"].get<std::vector<std::string>>();
        auto it = std::find(order.begin(), order.end(), modId);
        if (it != order.end()) {
            order.erase(it);
            contJson["load_order"] = order;
            std::ofstream outFile(contJsonPath);
            outFile << contJson.dump(4);
            cli::printSuccess("ModuleInstance '" + modId + "' removed from container '" + contName + "'");
        }
    } catch (...) {}
}

// --- Copy Container ---
void Arche::doCpCont(const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cerr << "Error: Src and Dest required.\n"; return; }
    fs::copy(projectRoot_ / "container" / args[0], projectRoot_ / "container" / args[1], fs::copy_options::recursive);
    cli::printSuccess("Container cloned successfully");
}

// --- Remove Container ---
void Arche::doRmCont(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "Error: Name required.\n"; return; }
    fs::remove_all(projectRoot_ / "container" / args[0]);
    cli::printSuccess("Container deleted");
}

// --- Archive Container ---
void Arche::doArcCont(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "Error: Name required.\n"; return; }
    std::string name = args[0];
    std::string cmd = "tar -czvf " + name + ".tar.gz -C " + (projectRoot_ / "container").string() + " " + name;
    runCommand(cmd);
}

// --- Unarchive Container ---
void Arche::doUnarcCont(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "Error: Path required.\n"; return; }
    fs::path archivePath(args[0]);
    std::string filename = archivePath.filename().string();
    std::string containerName = filename;
    size_t pos = filename.find(".tar.gz");
    if (pos != std::string::npos) containerName = filename.substr(0, pos);
    std::string cmd = "tar -xzvf " + archivePath.string() + " -C " + (projectRoot_ / "container").string();
    runCommand(cmd);
    std::cout << "Container '" << containerName << "' unpacked.\n";
}
