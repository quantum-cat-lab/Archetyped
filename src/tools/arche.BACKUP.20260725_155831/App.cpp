#include "App.h"
#include "CLIUtils.h"
#include "commands/CmdModule.h"
#include "commands/CmdContainer.h"
#include "commands/CmdBuild.h"
#include "commands/CmdSDK.h"
#include "commands/CmdUtil.h"

#include <iostream>
#include <sstream>

#define ARCHE_VERSION "0.0.10"

App::App() {
    // ModuleInstance commands
    addCommand(std::make_unique<CmdModuleList>());
    addCommand(std::make_unique<CmdModuleInfo>());
    addCommand(std::make_unique<CmdModuleInit>());
    addCommand(std::make_unique<CmdModuleUpdate>());
    addCommand(std::make_unique<CmdModuleScan>());

    // Container commands
    addCommand(std::make_unique<CmdContainerInit>());
    addCommand(std::make_unique<CmdContainerInfo>());
    addCommand(std::make_unique<CmdContainerAdd>());
    addCommand(std::make_unique<CmdContainerRemove>());
    addCommand(std::make_unique<CmdContainerCp>());
    addCommand(std::make_unique<CmdContainerRm>());
    addCommand(std::make_unique<CmdContainerArc>());
    addCommand(std::make_unique<CmdContainerUnarc>());

    // Build commands
    addCommand(std::make_unique<CmdEngineBuild>());
    addCommand(std::make_unique<CmdArcheBuild>());
    addCommand(std::make_unique<CmdBuild>());
    addCommand(std::make_unique<CmdRun>());
    addCommand(std::make_unique<CmdWatch>());
    addCommand(std::make_unique<CmdClean>());

    // SDK commands
    addCommand(std::make_unique<CmdSyncSdk>());
    addCommand(std::make_unique<CmdCheckSdk>());
    addCommand(std::make_unique<CmdInitSdk>());
    addCommand(std::make_unique<CmdDeps>());

    // Utility commands
    addCommand(std::make_unique<CmdDoctor>());
    addCommand(std::make_unique<CmdStatus>());
    addCommand(std::make_unique<CmdCompletions>());
    addCommand(std::make_unique<CmdTest>());

    // Aliases
    auto alias = [&](const std::string& a, const std::string& target) {
        // We'll handle aliases via dispatch fallback
    };
    (void)alias;
}

void App::addCommand(std::unique_ptr<ICommand> cmd) {
    if (!cmd) return;
    std::string n = cmd->name();
    registry_[n] = std::move(cmd);
}

ICommand* App::findCommand(const std::string& name) const {
    auto it = registry_.find(name);
    return it != registry_.end() ? it->second.get() : nullptr;
}

void App::dispatch(const std::string& cmdName, const Args& args) {
    // Handle help before lookup
    if (cmdName == "help") {
        std::string sub = args.pos(0);
        if (sub.empty()) printHelp();
        else printHelp(sub);
        return;
    }

    // Check for --help in args
    if (args.has("--help") || args.has("-h")) {
        printHelp(cmdName);
        return;
    }

    ICommand* cmd = findCommand(cmdName);
    if (cmd) {
        cmd->execute(args, ws_);
    } else {
        std::cout << cli::red("✘") << " Unknown command: " << cmdName
                  << "  " << cli::gray("Try 'arche help'") << "\n";
    }
}

int App::run(int argc, char* argv[]) {
    if (argc < 2) {
        // REPL mode
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
            std::vector<std::string> raw(parts.begin() + 1, parts.end());
            Args args = Args::parse(raw);

            // Handle aliases
            static const std::unordered_map<std::string, std::string> aliases = {
                {"list-modules", "list-mods"}, {"init-module", "init-mod"},
                {"add-module", "add-mod"}, {"rem-module", "rem-mod"},
                {"build-container", "build"}, {"run-container", "run"},
                {"copy-container", "cp-cont"}, {"remove-container", "rm-cont"},
                {"archive-container", "arc-cont"}, {"unarchive-container", "unarc-cont"},
                {"deps-graph", "deps"}, {"---help", "help"},
            };
            auto aliasIt = aliases.find(cmdName);
            if (aliasIt != aliases.end()) cmdName = aliasIt->second;

            dispatch(cmdName, args);
        }
        return 0;
    }

    // CLI argument mode
    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-V")) {
        std::cout << "arche v" ARCHE_VERSION << " (Archetyped Toolchain)" << std::endl;
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        printHelp();
        return 0;
    }

    std::string cmdName = argv[1];
    std::vector<std::string> raw;
    for (int i = 2; i < argc; ++i) raw.emplace_back(argv[i]);
    Args args = Args::parse(raw);

    // Aliases in CLI mode
    static const std::unordered_map<std::string, std::string> aliases = {
        {"list-modules", "list-mods"}, {"init-module", "init-mod"},
        {"add-module", "add-mod"}, {"rem-module", "rem-mod"},
        {"build-container", "build"}, {"run-container", "run"},
        {"copy-container", "cp-cont"}, {"remove-container", "rm-cont"},
        {"archive-container", "arc-cont"}, {"unarchive-container", "unarc-cont"},
        {"deps-graph", "deps"}, {"---help", "help"},
    };
    auto aliasIt = aliases.find(cmdName);
    if (aliasIt != aliases.end()) cmdName = aliasIt->second;

    dispatch(cmdName, args);
    return 0;
}

void App::printHelp(const std::string& command) const {
    if (!command.empty()) {
        ICommand* cmd = findCommand(command);
        if (cmd) {
            cli::printInfo(cmd->name() + " — " + cmd->description());
            return;
        }
        // Fallback for known commands
        auto detail = [&](const std::string& n, const std::string& desc) {
            if (command == n || command == n.substr(0, command.size())) {
                cli::printInfo(desc);
            }
        };
        detail("engine-build", "engine-build [--recache]\n  Build the Archetyped Engine core. --recache clears CMake cache.");
        detail("list-mods", "list-mods\n  List all available modules.");
        detail("info-mod", "info-mod <name>\n  Show detailed module info.");
        detail("scan", "scan\n  Scan modules and update project DB.");
        detail("init-mod", "init-mod <name>\n  Create a new module from template.");
        detail("init-sdk", "init-sdk <module> <unit-name> [version]\n  Create SDK provider skeleton.");
        detail("update-mod", "update-mod <name>\n  Update module from template.");
        detail("init-cont", "init-cont <name>\n  Create a new container.");
        detail("info-cont", "info-cont <name>\n  Show container info.");
        detail("add-mod", "add-mod <cont> <mod>\n  Add module to container load order.");
        detail("rem-mod", "rem-mod <cont> <mod>\n  Remove module from container.");
        detail("build", "build <cont> [--update-assets] [--src-included] [--recache]\n  Build and pack modules into container.");
        detail("run", "run <cont>\n  Set active container and launch engine.");
        detail("cp-cont", "cp-cont <src> <dest>\n  Clone a container.");
        detail("rm-cont", "rm-cont <name>\n  Delete a container.");
        detail("arc-cont", "arc-cont <name>\n  Archive container (tar.gz).");
        detail("unarc-cont", "unarc-cont <path>\n  Unpack container archive.");
        detail("test", "test\n  Run SDK integration test.");
        detail("sync-sdk", "sync-sdk [--check] [--list] [--all]\n  Vendor SDK headers with transitive dep resolution.");
        detail("check-sdk", "check-sdk\n  Comprehensive SDK integrity check.");
        detail("deps", "deps [module]\n  Show SDK dependency graph.");
        detail("arche-build", "arche-build\n  Rebuild the arche toolchain.");
        detail("doctor", "doctor [--quick]\n  Run system diagnostics.");
        detail("completions", "completions <bash|zsh|fish>\n  Generate shell completion script.");
        detail("status", "status\n  Project dashboard — modules, containers, SDK health.");
        detail("clean", "clean [--all]\n  Clean build artifacts.");
        detail("watch", "watch <container>\n  Watch container sources and hot-rebuild on change.");
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
