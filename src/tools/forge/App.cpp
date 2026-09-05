#include "App.h"
#include "CLIUtils.h"
#include "commands/CmdModule.h"
#include "commands/CmdContainer.h"
#include "commands/CmdBuild.h"
#include "commands/CmdSDK.h"
#include "commands/CmdUtil.h"
#include "commands/CmdGenSchema.h"
#include "commands/CmdInit.h"

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include "LineEditor.h"

#define FORGE_VERSION "0.0.10"

App::App() {
    addCommand(std::make_unique<CmdModuleList>());
    addCommand(std::make_unique<CmdModuleInfo>());
    addCommand(std::make_unique<CmdModuleInit>());
    addCommand(std::make_unique<CmdModuleUpdate>());
    addCommand(std::make_unique<CmdModuleScan>());

    addCommand(std::make_unique<CmdContainerInit>());
    addCommand(std::make_unique<CmdContainerInfo>());
    addCommand(std::make_unique<CmdContainerAdd>());
    addCommand(std::make_unique<CmdContainerRemove>());
    addCommand(std::make_unique<CmdContainerCp>());
    addCommand(std::make_unique<CmdContainerRm>());
    addCommand(std::make_unique<CmdContainerArc>());
    addCommand(std::make_unique<CmdContainerUnarc>());

    addCommand(std::make_unique<CmdEngineBuild>());
    addCommand(std::make_unique<CmdArcheBuild>());
    addCommand(std::make_unique<CmdBuild>());
    addCommand(std::make_unique<CmdRun>());
    addCommand(std::make_unique<CmdWatch>());
    addCommand(std::make_unique<CmdClean>());

    addCommand(std::make_unique<CmdSyncSdk>());
    addCommand(std::make_unique<CmdCheckSdk>());
    addCommand(std::make_unique<CmdInitSdk>());
    addCommand(std::make_unique<CmdDeps>());

    addCommand(std::make_unique<CmdDoctor>());
    addCommand(std::make_unique<CmdStatus>());
    addCommand(std::make_unique<CmdCompletions>());
    addCommand(std::make_unique<CmdTest>());
    addCommand(std::make_unique<CmdGenSchema>());
    addCommand(std::make_unique<CmdInit>());
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

std::string App::resolveAlias(const std::string& name) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"list-modules", "list-mods"}, {"init-module", "init-mod"},
        {"add-module", "add-mod"}, {"rem-module", "rem-mod"},
        {"build-container", "build"}, {"run-container", "run"},
        {"copy-container", "cp-cont"}, {"remove-container", "rm-cont"},
        {"archive-container", "arc-cont"}, {"unarchive-container", "unarc-cont"},
        {"deps-graph", "deps"}, {"---help", "help"},
        // short forms
        {"ls", "list-mods"}, {"info", "info-mod"},
        {"init-mod-old", "init-mod"},  // legacy alias for the old `init` -> mod new
        {"b", "build"}, {"r", "run"}, {"st", "status"},
    };
    auto it = aliases.find(name);
    return it != aliases.end() ? it->second : name;
}

std::string App::suggestCommand(const std::string& name) const {
    static constexpr int MAX_DIST = 2;
    std::string best;
    int bestDist = MAX_DIST + 1;
    for (auto& [cmdName, _] : registry_) {
        size_t n = name.size(), m = cmdName.size();
        std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1));
        for (size_t i = 0; i <= n; ++i) d[i][0] = (int)i;
        for (size_t j = 0; j <= m; ++j) d[0][j] = (int)j;
        for (size_t i = 1; i <= n; ++i)
            for (size_t j = 1; j <= m; ++j) {
                int sub = d[i-1][j-1] + (name[i-1] == cmdName[j-1] ? 0 : 1);
                d[i][j] = std::min({d[i-1][j] + 1, d[i][j-1] + 1, sub});
            }
        if (d[n][m] < bestDist) { bestDist = d[n][m]; best = cmdName; }
    }
    return bestDist <= MAX_DIST ? best : "";
}

int App::dispatch(const std::string& rawName, const Args& args) {
    if (rawName == "help") {
        std::string sub = args.pos(0);
        if (sub.empty()) printHelp();
        else printHelp(sub);
        return 0;
    }

    if (args.has("--help") || args.has("-h")) {
        printHelp(rawName);
        return 0;
    }

    // Noun-group syntax: `forge <group> <verb> ...` maps onto the flat
    // command registry, e.g. `cont build test` -> CmdBuild(test).
    static const std::unordered_map<std::string,
        std::map<std::string, std::pair<std::string, size_t>>> groups = {
        {"mod", {
            {"list",  {"list-mods", 0}},
            {"ls",    {"list-mods", 0}},
            {"info",  {"info-mod", 1}},
            {"new",   {"init-mod", 1}},
            {"init",  {"init-mod", 1}},
            {"update",{"update-mod", 1}},
            {"scan",  {"scan", 0}},
        }},
        {"cont", {
            {"new",    {"init-cont", 1}},
            {"init",   {"init-cont", 1}},
            {"info",   {"info-cont", 1}},
            {"add",    {"add-mod", 2}},     // cont add <c> <m>
            {"rm",     {"rem-mod", 2}},
            {"build",  {"build", 1}},
            {"run",    {"run", 1}},
            {"cp",     {"cp-cont", 2}},     // cont cp <src> <dest>
            {"clone",  {"cp-cont", 2}},
            {"delete", {"rm-cont", 1}},
            {"archive",{"arc-cont", 1}},
            {"unarchive",{"unarc-cont", 1}},
        }},
        {"sdk", {
            {"init",  {"init-sdk", 2}},     // sdk init <module> <unit>
            {"sync",  {"sync-sdk", 0}},
            {"check", {"check-sdk", 0}},
            {"deps",  {"deps", 1}},
        }},
    };

    std::vector<std::string> positional;
    for (auto& p : args.positional) positional.push_back(p);

    std::string cmdName = resolveAlias(rawName);
    auto gIt = groups.find(cmdName);
    if (gIt != groups.end()) {
        if (positional.empty()) {
            // `forge cont` alone -> group help
            printHelp(cmdName);
            return 0;
        }
        // Raw verb first; alias resolution only as fallback so short
        // top-level aliases ('info' -> 'info-mod') don't shadow group verbs.
        std::string verb = positional[0];
        auto vIt = gIt->second.find(verb);
        if (vIt == gIt->second.end()) {
            std::string aliased = resolveAlias(verb);
            vIt = gIt->second.find(aliased);
            if (vIt != gIt->second.end()) verb = aliased;
        }
        if (vIt == gIt->second.end()) {
            std::cout << cli::red("✘") << " Unknown verb: " << rawName << " " << verb << "\n";
            std::cout << "  " << cli::gray("Try 'forge help " + rawName + "'") << "\n";
            return 1;
        }
        cmdName = vIt->second.first;
        size_t minArgs = vIt->second.second;
        if (positional.size() - 1 < minArgs) {
            std::cout << cli::red("✘") << " Missing arguments: " << rawName << " " << verb
                      << cli::gray("  (needs " + std::to_string(minArgs) + " more)") << "\n";
            return 1;
        }
        // Rebuild args: verbs consumed the first positional.
        Args sub;
        sub.positional.assign(positional.begin() + 1, positional.end());
        sub.flags = args.flags;
        ICommand* cmd = findCommand(cmdName);
        return cmd->execute(sub, ws_);
    }

    ICommand* cmd = findCommand(cmdName);
    if (!cmd && rawName != cmdName) cmd = findCommand(rawName);
    if (cmd) {
        return cmd->execute(args, ws_);
    }

    std::cout << cli::red("✘") << " Unknown command: " << rawName << "\n";
    std::string sugg = suggestCommand(cmdName);
    if (sugg.empty()) sugg = suggestCommand(rawName);
    if (!sugg.empty())
        std::cout << "  " << cli::gray("Did you mean") << " " << cli::bold(cli::cyan(sugg)) << "?\n";
    std::cout << "  " << cli::gray("Try 'forge help'") << "\n";
    return 1;
}

int App::run(int argc, char* argv[]) {
    // Outside a project: refuse commands that need the workspace instead of
    // scanning arbitrary directories (the old fallback hung on /var, / etc).
    if (!ws_.valid() && argc >= 2) {
        std::string c = argv[1];
        bool harmless = (c == "--help" || c == "-h" || c == "--version" || c == "-V" || c == "help");
        if (!harmless) {
            std::cout << cli::red("✘") << " Not in an Archetyped project (no CMakeLists.txt + container/ above cwd).\n";
            std::cout << "  " << cli::gray("cd into the project root and retry") << "\n";
            return 2;
        }
    }

    if (argc < 2) {
        cli::printLogo(FORGE_VERSION);
        std::string activeCont; // persistent container context ('use <name>')

        auto tokenize = [](const std::string& line) {
            // Whitespace split with double/single-quote support.
            std::vector<std::string> tok;
            std::string cur;
            char quote = 0;
            for (char ch : line) {
                if (quote) {
                    if (ch == quote) quote = 0; else cur += ch;
                } else if (ch == '"' || ch == '\'') {
                    quote = ch;
                } else if (isspace((unsigned char)ch)) {
                    if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
                } else {
                    cur += ch;
                }
            }
            if (!cur.empty()) tok.push_back(cur);
            return tok;
        };

        LineEditor editor;
        std::string input;
        while (true) {
            std::string pfx = cli::gray("[ ") + cli::bold(cli::cyan("forge")) + " ";
            if (!activeCont.empty()) pfx += cli::magenta("(" + activeCont + ")") + " ";
            pfx += cli::green("●") + " " + cli::gray("] ");
            if (!editor.read(pfx, input)) break;

            if (input == "exit" || input == "quit" || input == ":q") break;
            if (input == "clear") { std::cout << "\033[2J\033[H"; continue; }
            if (input.empty()) continue;

            std::vector<std::string> parts = tokenize(input);

            if (parts[0] == "use") {
                if (parts.size() == 1) {
                    if (activeCont.empty())
                        std::cout << cli::gray("  No container selected. Usage: use <container>") << "\n";
                    else
                        std::cout << "  " << cli::gray("Context:") << " " << cli::bold(activeCont) << "\n";
                } else if (parts[1] == ".." || parts[1] == "off") {
                    activeCont.clear();
                    std::cout << "  " << cli::gray("Context cleared") << "\n";
                } else {
                    activeCont = parts[1];
                    std::cout << "  " << cli::gray("Context set to") << " " << cli::bold(cli::cyan(activeCont)) << "\n";
                }
                continue;
            }

            std::string cmdName = parts[0];
            Args args;
            for (size_t i = 1; i < parts.size(); ++i)
                args.positional.push_back(parts[i]);
            // If a container context exists and the command's first positional
            // is missing, inject it so bare 'run' / 'build' just work.
            static const std::unordered_set<std::string> contFirst = {
                "build", "run", "info-cont", "cp-cont", "rm-cont",
                "arc-cont", "watch",
            };
            if (!activeCont.empty() && contFirst.count(cmdName) && args.positional.empty()) {
                args.positional.push_back(activeCont);
            }
            dispatch(cmdName, args);
        }
        return 0;
    }

    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-V")) {
        std::cout << "forge v" FORGE_VERSION << " (Archetyped Forge)" << std::endl;
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
    return dispatch(cmdName, args);
}

void App::printHelp(const std::string& command) const {
    if (!command.empty()) {
        ICommand* cmd = findCommand(resolveAlias(command));
        if (cmd) {
            cli::printInfo(cmd->name() + " — " + cmd->description());
            return;
        }

        // Group help: list verbs for a noun-group.
        static const std::map<std::string, std::vector<std::pair<std::string, std::string>>> groupHelp = {
            {"mod", {
                {"mod list",           "List all modules"},
                {"mod info <name>",    "ModuleInstance info"},
                {"mod new <name>",     "Create module from template"},
                {"mod update <name>",  "Update from template"},
                {"mod scan",           "Scan modules, update DB"},
            }},
            {"cont", {
                {"cont new <name>",         "Create a container"},
                {"cont info <name>",        "Container info"},
                {"cont add <c> <m>",        "Add module to container"},
                {"cont rm <c> <m>",         "Remove module from container"},
                {"cont build <name>",       "Build container"},
                {"cont run <name>",         "Launch engine"},
                {"cont cp <src> <dest>",    "Clone container"},
                {"cont delete <name>",      "Delete container"},
                {"cont archive <name>",     "Archive container (tar.gz)"},
                {"cont unarchive <path>",   "Unpack archive"},
            }},
            {"sdk", {
                {"sdk init <module> <unit> [ver]", "Create SDK provider skeleton"},
                {"sdk sync [opts]",         "Vendor SDK headers"},
                {"sdk check",               "Verify SDK integrity"},
                {"sdk deps [module]",       "SDK dependency graph"},
            }},
            {"build", {
                {"engine-build",       "Build engine core"},
                {"forge-build",       "Rebuild forge toolchain"},
            }},
        };
        auto gh = groupHelp.find(command);
        if (gh != groupHelp.end()) {
            std::string icon = cli::iconGear();
            if (command == "cont") icon = cli::iconContainer();
            else if (command == "sdk") icon = cli::iconSDK();
            else if (command == "build") icon = cli::iconBuild();
            cli::printSection(icon, command + " Commands");
            size_t w = 0;
            for (auto& [u, d] : gh->second) w = std::max(w, u.size());
            for (auto& [u, d] : gh->second) {
                std::cout << "    " << cli::iconBullet() << " " << cli::bold(u)
                          << std::string(w - u.size() + 2, ' ') << cli::gray(d) << "\n";
            }
            return;
        }

        std::cout << cli::red("✘") << " Unknown command: " << command << "\n";
        return;
    }

    cli::printRule();
    cli::printSection(cli::iconGear(), "Modules");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("mod list") << "              " << cli::gray("List all modules") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("mod info <name>") << "        " << cli::gray("ModuleInstance info") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("mod new <name>") << "         " << cli::gray("Create module from template") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("mod update <name>") << "      " << cli::gray("Update from template") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("mod scan") << "               " << cli::gray("Scan modules, update DB") << "\n";

    cli::printSection(cli::iconContainer(), "Containers");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont new <name>") << "        " << cli::gray("Create a container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont info <name>") << "       " << cli::gray("Container info") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont add <c> <m>") << "       " << cli::gray("Add module to container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont rm <c> <m>") << "        " << cli::gray("Remove module from container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont build <name>") << "      " << cli::gray("Build container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont run <name>") << "        " << cli::gray("Launch engine") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont cp <src> <dest>") << "   " << cli::gray("Clone container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont delete <name>") << "     " << cli::gray("Delete container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont archive <name>") << "    " << cli::gray("Archive container") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("cont unarchive <path>") << "  " << cli::gray("Unpack archive") << "\n";

    cli::printSection(cli::iconSDK(), "SDK");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("sdk init <m> <u> [ver]") << " " << cli::gray("Create SDK provider") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("sdk sync [opts]") << "        " << cli::gray("Vendor SDK headers") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("sdk check") << "              " << cli::gray("Verify SDK integrity") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("sdk deps [module]") << "      " << cli::gray("SDK dependency graph") << "\n";

    cli::printSection(cli::iconBuild(), "Build Commands");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("engine-build") << " [opts]" << "   " << cli::gray("Build engine core") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("forge-build") << "            " << cli::gray("Rebuild forge toolchain") << "\n";

    cli::printSection(cli::iconStar(), "Utilities");
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("doctor") << "               " << cli::gray("System diagnostics") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("status") << "               " << cli::gray("Project status") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("init") << "   [--reset]    " << cli::gray("Initialize workspace configs (idempotent)") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("clean") << " [--all]" << "        " << cli::gray("Clean artifacts") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("completions") << " <shell>" << "   " << cli::gray("Shell completions") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("test") << "                " << cli::gray("Run SDK integration test") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("watch") << " <cont>" << "        " << cli::gray("Hot-reload on file change") << "\n";
    std::cout << "    " << cli::iconBullet() << " " << cli::bold("help") << " [cmd]" << "          " << cli::gray("Show help") << "\n";

    cli::printRule();
    std::cout << "  " << cli::gray("Usage: forge <command> [args]  |  v" FORGE_VERSION " | Archetyped") << "\n\n";
}
