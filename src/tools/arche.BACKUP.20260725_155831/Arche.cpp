#include "Arche.h"

Arche::Arche() {
    projectRoot_ = fs::current_path();
    modulesRoot_ = projectRoot_.parent_path();
    registerCommands();
}

bool Arche::runCommand(const std::string& cmd) {
    std::cout << "Executing: " << cmd << std::endl;
    int result = std::system(cmd.c_str());
    return result == 0;
}

fs::path Arche::resolveModuleDir(const std::string& modId) const {
    if (fs::exists(modulesRoot_ / modId / "module.json"))
        return modulesRoot_ / modId;
    for (const auto& entry : fs::directory_iterator(modulesRoot_)) {
        if (entry.is_directory() && entry.path() != projectRoot_) {
            fs::path jsonPath = entry.path() / "module.json";
            if (fs::exists(jsonPath)) {
                try {
                    std::ifstream file(jsonPath);
                    json mJson; file >> mJson;
                    if (mJson.value("id", "") == modId)
                        return entry.path();
                } catch (...) {}
            }
        }
    }
    return {};
}

void Arche::replaceInFile(const fs::path& filePath, const std::string& oldStr, const std::string& newStr) {
    try {
        std::ifstream inFile(filePath);
        if (!inFile) return;
        std::string content((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        size_t pos = 0;
        bool changed = false;
        while ((pos = content.find(oldStr, pos)) != std::string::npos) {
            content.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
            changed = true;
        }

        if (changed) {
            std::ofstream outFile(filePath);
            outFile << content;
        }
    } catch (...) {}
}

std::string Arche::macroIdent(const std::string& name) {
    std::string out;
    for (unsigned char ch : name)
        out.push_back(std::isalnum(ch) ? (char)std::toupper(ch) : '_');
    return out;
}

void Arche::addModuleToWorkspace(const std::string& name) {
    fs::path wsJson = projectRoot_ / "configs" / "module_workspace.json";
    if (!fs::exists(wsJson)) return;
    std::ifstream wf(wsJson); json ws; wf >> ws;
    for (auto& mod : ws["modules"])
        if (mod.value("name","") == name) return;
    ws["modules"].push_back({{"name", name}, {"path", name}});
    std::ofstream of(wsJson); of << ws.dump(2) << std::endl;
}

void Arche::printHelp(const std::string& command) {
    if (!command.empty()) {
        if (command == "engine-build") cli::printInfo("engine-build [--recache]\n  Build the Archetyped Engine core. --recache clears CMake cache.");
        else if (command == "list-mods") cli::printInfo("list-mods\n  List all available modules.");
        else if (command == "info-mod") cli::printInfo("info-mod <name>\n  Show detailed module info.");
        else if (command == "scan") cli::printInfo("scan\n  Scan modules and update project DB.");
        else if (command == "init-mod") cli::printInfo("init-mod <name>\n  Create a new module from template.");
        else if (command == "init-sdk") cli::printInfo("init-sdk <module> <unit-name> [version]\n  Create SDK provider skeleton.");
        else if (command == "update-mod") cli::printInfo("update-mod <name>\n  Update module from template.");
        else if (command == "init-cont") cli::printInfo("init-cont <name>\n  Create a new container.");
        else if (command == "info-cont") cli::printInfo("info-cont <name>\n  Show container info.");
        else if (command == "add-mod") cli::printInfo("add-mod <cont> <mod>\n  Add module to container load order.");
        else if (command == "rem-mod") cli::printInfo("rem-mod <cont> <mod>\n  Remove module from container.");
        else if (command == "build") cli::printInfo("build <cont> [--update-assets] [--src-included] [--recache]\n  Build and pack modules into container.");
        else if (command == "run") cli::printInfo("run <cont>\n  Set active container and launch engine.");
        else if (command == "cp-cont") cli::printInfo("cp-cont <src> <dest>\n  Clone a container.");
        else if (command == "rm-cont") cli::printInfo("rm-cont <name>\n  Delete a container.");
        else if (command == "arc-cont") cli::printInfo("arc-cont <name>\n  Archive container (tar.gz).");
        else if (command == "unarc-cont") cli::printInfo("unarc-cont <path>\n  Unpack container archive.");
        else if (command == "test") cli::printInfo("test\n  Run SDK integration test.");
        else if (command == "sync-sdk") cli::printInfo("sync-sdk [--check] [--list] [--all]\n  Vendor SDK headers with transitive dep resolution.");
        else if (command == "check-sdk") cli::printInfo("check-sdk\n  Comprehensive SDK integrity check.");
        else if (command == "deps") cli::printInfo("deps [module]\n  Show SDK dependency graph.");
        else if (command == "arche-build") cli::printInfo("arche-build\n  Rebuild the arche toolchain.");
        else if (command == "doctor") cli::printInfo("doctor [--quick]\n  Run system diagnostics.");
        else if (command == "completions") cli::printInfo("completions <bash|zsh|fish>\n  Generate shell completion script.");
            else if (command == "status") cli::printInfo("status\n  Project dashboard — modules, containers, SDK health.");
            else if (command == "clean") cli::printInfo("clean [--all]\n  Clean build artifacts.");
            else if (command == "watch") cli::printInfo("watch <container>\n  Watch container sources and hot-rebuild on change.");
        else cli::printError("Unknown command: " + command);
        return;
    }

    cli::printRule();
    cli::printSection(cli::iconGear(), "ModuleInstance Commands");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("scan") << "               " << cli::gray("Scan modules, update DB") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("list-mods") << "          " << cli::gray("List all modules") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("info-mod") << " <name>" << "  " << cli::gray("ModuleInstance info") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("init-mod") << " <name>" << "  " << cli::gray("Create module from template") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("update-mod") << " <name>" << cli::gray("  Update from template") << "\n";

    cli::printSection(cli::iconContainer(), "Container Commands");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("init-cont") << " <name>" << " " << cli::gray("Create a container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("info-cont") << " <name>" << " " << cli::gray("Container info") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("add-mod") << " <c> <m>" << "  " << cli::gray("Add module to container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("rem-mod") << " <c> <m>" << "  " << cli::gray("Remove module from container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("build") << " <name>" << "     " << cli::gray("Build container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("run") << " <name>" << "       " << cli::gray("Launch engine") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cp-cont") << " <s> <d>" << " " << cli::gray("Clone container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("rm-cont") << " <name>" << "   " << cli::gray("Delete container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("arc-cont") << " <name>" << "  " << cli::gray("Archive container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("unarc-cont") << " <path>" << " " << cli::gray("Unpack archive") << "\n";

    cli::printSection(cli::iconSDK(), "SDK Commands");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("init-sdk") << " <m> <u> [ver]" << cli::gray("  Create SDK provider") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("sync-sdk") << " [opts]" << "     " << cli::gray("Vendor SDK headers") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("check-sdk") << "            " << cli::gray("Verify SDK integrity") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("deps") << " [module]" << "        " << cli::gray("SDK dependency graph") << "\n";

    cli::printSection(cli::iconBuild(), "Build Commands");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("engine-build") << " [opts]" << "   " << cli::gray("Build engine core") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("arche-build") << "           " << cli::gray("Rebuild arche toolchain") << "\n";

    cli::printSection(cli::iconStar(), "Utilities");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("doctor") << "               " << cli::gray("System diagnostics") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("status") << "               " << cli::gray("Project status") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("clean") << " [--all]" << "        " << cli::gray("Clean artifacts") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("completions") << " <shell>" << "   " << cli::gray("Shell completions") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("test") << "                " << cli::gray("Run SDK integration test") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("watch") << " <cont>" << "        " << cli::gray("Hot-reload on file change") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("help") << " [cmd]" << "          " << cli::gray("Show help") << "\n";

    cli::printRule();
    std::cout << "  " << cli::gray("Usage: arche <command> [args]  |  v" ARCHE_VERSION " | Archetyped") << "\n\n";
}

void Arche::registerCommands() {
    commandRegistry_["engine-build"] = [this](auto& args) { doEngineBuild(args); };
    commandRegistry_["arche-build"] = [this](auto& args) { doArcheBuild(args); };
    commandRegistry_["list-mods"] = [this](auto& args) { doListMods(args); };
    commandRegistry_["info-mod"] = [this](auto& args) { doInfoMod(args); };
    commandRegistry_["scan"] = [this](auto& args) { doScan(args); };
    commandRegistry_["init-mod"] = [this](auto& args) { doInitMod(args); };
    commandRegistry_["update-mod"] = [this](auto& args) { doUpdateMod(args); };
    commandRegistry_["init-module"] = [this](auto& args) { doInitMod(args); };
    commandRegistry_["init-cont"] = [this](auto& args) { doInitCont(args); };
    commandRegistry_["init-container"] = [this](auto& args) { doInitCont(args); };
    commandRegistry_["info-cont"] = [this](auto& args) { doInfoCont(args); };
    commandRegistry_["add-mod"] = [this](auto& args) { doAddMod(args); };
    commandRegistry_["add-module"] = [this](auto& args) { doAddMod(args); };
    commandRegistry_["rem-mod"] = [this](auto& args) { doRemMod(args); };
    commandRegistry_["rem-module"] = [this](auto& args) { doRemMod(args); };
    commandRegistry_["build"] = [this](auto& args) {
        if (args.empty()) return;
        doBuild(args);
    };
    commandRegistry_["build-container"] = [this](auto& args) {
        if (args.empty()) return;
        doBuild(args);
    };
    commandRegistry_["run"] = [this](auto& args) { doRun(args); };
    commandRegistry_["run-container"] = [this](auto& args) { doRun(args); };
    commandRegistry_["cp-cont"] = [this](auto& args) { if (args.size() >= 2) doCpCont(args); };
    commandRegistry_["copy-container"] = [this](auto& args) { if (args.size() >= 2) doCpCont(args); };
    commandRegistry_["rm-cont"] = [this](auto& args) { if (!args.empty()) doRmCont(args); };
    commandRegistry_["remove-container"] = [this](auto& args) { if (!args.empty()) doRmCont(args); };
    commandRegistry_["arc-cont"] = [this](auto& args) { if (!args.empty()) doArcCont(args); };
    commandRegistry_["archive-container"] = [this](auto& args) { if (!args.empty()) doArcCont(args); };
    commandRegistry_["unarc-cont"] = [this](auto& args) { if (!args.empty()) doUnarcCont(args); };
    commandRegistry_["unarchive-container"] = [this](auto& args) { if (!args.empty()) doUnarcCont(args); };
    commandRegistry_["test"] = [this](auto& args) { doTest(args); };
    commandRegistry_["sync-sdk"] = [this](auto& args) { doSyncSdk(args); };
    commandRegistry_["check-sdk"] = [this](auto& args) { doCheckSdk(args); };
    commandRegistry_["init-sdk"] = [this](auto& args) { doInitSdk(args); };
    commandRegistry_["deps"] = [this](auto& args) { doDeps(args); };
    commandRegistry_["deps-graph"] = [this](auto& args) { doDeps(args); };
    commandRegistry_["doctor"] = [this](auto& args) { doDoctor(args); };
    commandRegistry_["completions"] = [this](auto& args) { doCompletions(args); };
    commandRegistry_["status"] = [this](auto& args) { doStatus(args); };
    commandRegistry_["clean"] = [this](auto& args) { doClean(args); };
    commandRegistry_["watch"] = [this](auto& args) { doWatch(args); };
}

int Arche::run(int argc, char* argv[]) {
    if (argc < 2) {
        cli::printLogo(ARCHE_VERSION);
        std::string input;
        while (true) {
            cli::printPrompt();
            std::getline(std::cin, input);
            if (input == "exit" || input == "quit") break;
            if (input == "clear") { std::cout << "\033[2J\033[H"; continue; }
            if (input.empty()) continue;

            std::vector<std::string> parts;
            size_t pos = 0;
            while ((pos = input.find(' ')) != std::string::npos) {
                parts.push_back(input.substr(0, pos));
                input.erase(0, pos + 1);
            }
            parts.push_back(input);

            std::string cmdName = parts[0];
            std::vector<std::string> args(parts.begin() + 1, parts.end());

            if (cmdName == "help") { printHelp(); }
            else if (!args.empty() && args.back() == "--help") {
                printHelp(cmdName);
            } else {
                auto it = commandRegistry_.find(cmdName);
                if (it != commandRegistry_.end()) it->second(args);
                else std::cout << cli::red("✘") << " Unknown command: " << cmdName << "  " << cli::gray("Try 'help'") << "\n";
            }
        }
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-V")) {
        std::cout << "arche v" ARCHE_VERSION << " (Archetyped Toolchain)" << std::endl;
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        printHelp();
        return 0;
    }
    std::string cmdName = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.push_back(argv[i]);
    if (cmdName == "help" || cmdName == "---help") {
        printHelp((args.empty() ? "" : args[0]));
    } else if (!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
        printHelp(cmdName);
    } else {
        auto it = commandRegistry_.find(cmdName);
        if (it != commandRegistry_.end()) it->second(args);
        else { std::cout << cli::red("✘") << " Unknown command: " << cmdName << "  " << cli::gray("Try 'arche help'") << "\n"; }
    }
    return 0;
}
