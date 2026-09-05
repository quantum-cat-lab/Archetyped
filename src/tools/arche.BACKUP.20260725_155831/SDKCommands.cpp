#include "Arche.h"
#include "SHA256.h"

// --- SDK Provider: sync-sdk ---
void Arche::doSyncSdk(const std::vector<std::string>& args) {
    bool checkMode = false, listMode = false, allMode = false;
    std::string targetMod;
    for (const auto& a : args) {
        if (a == "--check") checkMode = true;
        else if (a == "--list") listMode = true;
        else if (a == "--all") allMode = true;
        else if (!a.empty() && a[0] != '-') targetMod = a;
    }
    fs::path wsRoot = modulesRoot_;
    fs::path wsJson = projectRoot_ / "configs" / "module_workspace.json";
    if (!fs::exists(wsJson)) {
        std::cerr << "arche: error: module_workspace.json not found at " << wsRoot << "\n";
        return;
    }
    std::ifstream wf(wsJson); json ws; wf >> ws;
    if (!ws.contains("modules") || !ws["modules"].is_array()) {
        std::cerr << "arche: error: module_workspace.json has no 'modules'\n"; return;
    }

    // provider registry: name -> list of (version, providerName, providerPath, headers)
    std::map<std::string, std::vector<std::tuple<std::string,std::string,std::string,std::vector<std::string>>>> byName;
    for (auto& mod : ws["modules"]) {
        if (!mod.contains("path")) continue;
        std::string mpath = (wsRoot / mod.value("path","")).string();
        fs::path mmanifest = fs::path(mpath) / "configs" / "sdk-manifest.json";
        if (!fs::exists(mmanifest)) continue;
        std::ifstream mf(mmanifest); json mj; mf >> mj;
        if (mj.contains("provides") && mj["provides"].is_array()) {
            for (auto& prov : mj["provides"]) {
                std::string name = prov.value("name","");
                std::string version = prov.value("version","");
                std::vector<std::string> headers;
                if (prov.contains("headers") && prov["headers"].is_array())
                    for (auto& h : prov["headers"]) headers.push_back(h.get<std::string>());
                byName[name].emplace_back(version, mod.value("name",""), mpath, headers);
            }
        }
    }

    bool conflict = false;
    for (auto& [name, vers] : byName) {
        std::set<std::string> distinct;
        std::map<std::string, std::set<std::string>> verMods;
        for (auto& v : vers) {
            distinct.insert(std::get<0>(v));
            verMods[std::get<0>(v)].insert(std::get<1>(v));
        }
        if (distinct.size() > 1) {
            std::cerr << "arche: conflict: version conflict for SDK unit '" << name << "'\n";
            conflict = true;
        }
        for (auto& [ver, mods] : verMods)
            if (mods.size() > 1) {
                std::cerr << "arche: conflict: ambiguous provider for '" << name << "@" << ver << "'\n";
                conflict = true;
            }
    }
    if (conflict) return;

    auto findProvider = [&](const std::string& name, const std::string& ver,
                            std::string& pMod, std::string& pPath, std::vector<std::string>& headers) -> bool {
        auto it = byName.find(name);
        if (it == byName.end()) return false;
        for (auto& v : it->second)
            if (std::get<0>(v) == ver) { pMod = std::get<1>(v); pPath = std::get<2>(v); headers = std::get<3>(v); return true; }
        return false;
    };

    struct Spec { std::string unit, version, providerModule, providerPath, modulePath; std::vector<std::string> headers; };
    std::map<std::string, std::vector<Spec>> resolved;
    bool problem = false;
    for (auto& mod : ws["modules"]) {
        if (!mod.contains("path") || !mod.contains("name")) continue;
        std::string mname = mod.value("name","");
        if (!targetMod.empty() && mname != targetMod) continue;
        std::string mpath = (wsRoot / mod.value("path","")).string();
        fs::path mmanifest = fs::path(mpath) / "configs" / "sdk-manifest.json";
        if (!fs::exists(mmanifest)) continue;
        std::ifstream mf(mmanifest); json mj; mf >> mj;
        std::vector<std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>> direct;
        if (mj.contains("requires") && mj["requires"].is_array()) {
            for (auto& req : mj["requires"]) {
                std::string name = req.value("name","");
                std::string ver = req.value("version","");
                std::string pMod, pPath; std::vector<std::string> headers;
                if (!findProvider(name, ver, pMod, pPath, headers)) {
                    std::cerr << "arche: conflict: module '" << mname << "' requires '"
                              << name << "@" << ver << "' but not satisfied\n";
                    problem = true; continue;
                }
                direct.emplace_back(name, ver, pMod, pPath, headers);
                resolved[mname].push_back({name, ver, pMod, pPath, mpath, headers});
            }
        }
        // Transitive resolution: also pull in deps-of-deps
        auto transitive = resolveTransitive(mpath, byName, direct, listMode);
        for (auto& t : transitive) {
            std::string tunit = std::get<0>(t);
            std::string tver = std::get<1>(t);
            std::string tmod = std::get<2>(t);
            std::string tpath = std::get<3>(t);
            auto& thdrs = std::get<4>(t);
            // Skip if already in resolved
            bool found = false;
            for (auto& r : resolved[mname])
                if (r.unit == tunit && r.version == tver) { found = true; break; }
            if (!found)
                resolved[mname].push_back({tunit, tver, tmod, tpath, mpath, thdrs});
        }
    }
    if (problem && !allMode) return;

    auto vendorDest = [&](const std::string& modulePath, const std::string& unit, const std::string& headerRel) -> fs::path {
        std::string prefix = "include/SDK/" + unit + "/";
        std::string rel = headerRel;
        if (rel.rfind(prefix, 0) == 0) rel = rel.substr(prefix.size());
        else rel = fs::path(headerRel).filename().string();
        return fs::path(modulePath) / prefix / rel;
    };

    if (listMode) {
        std::cout << "arche sync-sdk --list (workspace: " << wsRoot << ")\n";
        for (auto& [mname, specs] : resolved) {
            if (specs.empty()) continue;
            std::cout << mname << ":\n";
            for (auto& s : specs) {
                std::string hs;
                for (auto& h : s.headers) { if (!hs.empty()) hs += ", "; hs += fs::path(h).filename().string(); }
                if (hs.empty()) hs = "(none)";
                std::cout << "  " << s.unit << "@" << s.version << " <- " << s.providerModule << " [" << hs << "]\n";
            }
        }
        return;
    }

    if (checkMode) {
        std::cout << "arche sync-sdk --check (workspace: " << wsRoot << ")\n";
        bool ok = true;
        for (auto& [mname, specs] : resolved) {
            for (auto& s : specs) {
                for (auto& hrel : s.headers) {
                    fs::path src = fs::path(s.providerPath) / hrel;
                    fs::path dst = vendorDest(s.modulePath, s.unit, hrel);
                    if (!fs::exists(src)) { std::cerr << "  MISSING canonical: " << src << "\n"; ok = false; continue; }
                    if (!fs::exists(dst)) { std::cerr << "  DRIFT: vendored copy missing: " << dst << "\n"; ok = false; continue; }
                    std::string sh = sha256_file(src), dh = sha256_file(dst);
                    if (sh != dh) {
                        std::cerr << "  DRIFT: " << dst << "\n    canonical " << sh << " != vendored " << dh << "\n";
                        ok = false;
                    } else {
                        std::cout << "  ok: " << s.unit << "/" << fs::path(hrel).filename().string() << " (" << mname << ")\n";
                    }
                }
                fs::path mg = fs::path(s.modulePath) / "include" / "SDK" / s.unit / "SdkManifest.g.h";
                if (!fs::exists(mg)) { std::cerr << "  DRIFT: " << mg << " missing\n"; ok = false; }
            }
        }
        if (!ok) { std::cerr << "arche: --check FAILED (drift detected)\n"; std::exit(1); }
        cli::printSuccess("--check PASSED (all vendored copies match canonical)");
        return;
    }

    int written = 0, skipped = 0;
    std::cout << "arche sync-sdk (workspace: " << wsRoot << ")\n";
    for (auto& [mname, specs] : resolved) {
        for (auto& s : specs) {
            std::map<std::string,std::string> fileHashes;
            for (auto& hrel : s.headers) {
                fs::path src = fs::path(s.providerPath) / hrel;
                fs::path dst = vendorDest(s.modulePath, s.unit, hrel);
                if (!fs::exists(src)) { std::cerr << "arche: error: canonical header missing: " << src << "\n"; return; }
                fs::create_directories(dst.parent_path());
                std::ifstream in(src, std::ios::binary);
                std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                if (fs::exists(dst)) {
                    std::ifstream existing(dst, std::ios::binary);
                    std::string existingData((std::istreambuf_iterator<char>(existing)), std::istreambuf_iterator<char>());
                    if (existingData == data) {
                        fileHashes[fs::path(hrel).filename().string()] = sha256_file(src);
                        skipped++;
                        continue;
                    }
                }
                std::ofstream out(dst, std::ios::binary);
                out << data;
                fileHashes[fs::path(hrel).filename().string()] = sha256_file(src);
                std::cout << "  vendored " << s.unit << "/" << fs::path(hrel).filename().string() << " -> " << mname << "\n";
                written++;
            }
            fs::path outDir = fs::path(s.modulePath) / "include" / "SDK" / s.unit;
            fs::create_directories(outDir);
            std::string combined;
            for (auto& kv : fileHashes) combined += kv.second;
            std::string combinedHash = sha256_hex(combined);
            std::string unitMacro = macroIdent(s.unit);

            std::string mgContent;
            mgContent += "// Auto-generated by arche sync-sdk. DO NOT EDIT MANUALLY.\n";
            mgContent += "// Unit: " + s.unit + "  Version: " + s.version + "\n";
            mgContent += "#pragma once\n";
            mgContent += "#define SDK_" + unitMacro + "_VERSION \"" + s.version + "\"\n";
            for (auto& kv : fileHashes)
                mgContent += "#define SDK_" + unitMacro + "_FILE_" + macroIdent(kv.first) + "_HASH \"sha256:" + kv.second + "\"\n";
            mgContent += "#define SDK_" + unitMacro + "_HASH \"sha256:" + combinedHash + "\"\n";

            fs::path mgPath = outDir / "SdkManifest.g.h";
            if (fs::exists(mgPath)) {
                std::ifstream existingMg(mgPath);
                std::string existingMgContent((std::istreambuf_iterator<char>(existingMg)), std::istreambuf_iterator<char>());
                if (existingMgContent == mgContent) continue;
            }
            std::ofstream mg(mgPath);
            mg << mgContent;
            std::cout << "  wrote SdkManifest.g.h for " << s.unit << " (" << mname << ")\n";
        }
    }
    if (skipped > 0)
        std::cout << "  " << cli::gray("(" + std::to_string(skipped) + " file(s) unchanged, skipped)") << "\n";
    cli::printSuccess("sync-sdk complete");
}

// --- Transitive dependency resolver ---
std::vector<std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>>
Arche::resolveTransitive(const std::string& modulePath,
                  const std::map<std::string, std::vector<std::tuple<std::string,std::string,std::string,std::vector<std::string>>>>& byName,
                  const std::vector<std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>>& direct,
                  bool quiet) {
    std::map<std::string, std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>> result;
    std::vector<std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>> queue = direct;
    while (!queue.empty()) {
        auto item = queue.back(); queue.pop_back();
        std::string unit = std::get<0>(item);
        std::string ver = std::get<1>(item);
        std::string key = unit + "@" + ver;
        if (result.find(key) != result.end()) continue;
        result[key] = std::make_tuple(unit, ver, std::get<2>(item), std::get<3>(item), std::get<4>(item));
        // Look up provider's own configs/sdk-manifest.json to find its requires
        std::string providerPath = std::get<3>(item);
        fs::path pman = fs::path(providerPath) / "configs" / "sdk-manifest.json";
        if (fs::exists(pman)) {
            std::ifstream pf(pman); json pj; pf >> pj;
            if (pj.contains("requires") && pj["requires"].is_array()) {
                for (auto& req : pj["requires"]) {
                    std::string rname = req.value("name","");
                    std::string rver = req.value("version","");
                    std::string rkey = rname + "@" + rver;
                    if (result.find(rkey) != result.end()) continue;
                    auto it = byName.find(rname);
                    if (it == byName.end()) {
                        if (!quiet) std::cerr << "  warning: transitive dep '" << rname << "@" << rver << "' (required by " << unit << ") has no provider\n";
                        continue;
                    }
                    bool found = false;
                    for (auto& v : it->second) {
                        if (std::get<0>(v) == rver) {
                            queue.emplace_back(rname, rver, std::get<1>(v), std::get<2>(v), std::get<3>(v));
                            found = true; break;
                        }
                    }
                    if (!found && !quiet)
                        std::cerr << "  warning: transitive dep '" << rname << "@" << rver << "' required but version not found\n";
                }
            }
        }
    }
    std::vector<std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>> out;
    for (auto& kv : result) out.push_back(kv.second);
    return out;
}

// --- check-sdk: comprehensive SDK integrity check ---
void Arche::doCheckSdk(const std::vector<std::string>& args) {
    std::cout << "arche check-sdk (workspace: " << modulesRoot_ << ")\n\n";
    bool allOk = true;
    fs::path wsJson = projectRoot_ / "configs" / "module_workspace.json";
    if (!fs::exists(wsJson)) { std::cerr << "error: module_workspace.json not found\n"; return; }
    std::ifstream wf(wsJson); json ws; wf >> ws;

    // Build provider registry (same as sync-sdk)
    std::map<std::string, std::vector<std::tuple<std::string,std::string,std::string,std::vector<std::string>>>> byName;
    for (auto& mod : ws["modules"]) {
        if (!mod.contains("path")) continue;
        std::string mpath = (modulesRoot_ / mod.value("path","")).string();
        fs::path mmanifest = fs::path(mpath) / "configs" / "sdk-manifest.json";
        if (!fs::exists(mmanifest)) continue;
        std::ifstream mf(mmanifest); json mj; mf >> mj;
        if (mj.contains("provides") && mj["provides"].is_array()) {
            for (auto& prov : mj["provides"]) {
                std::string name = prov.value("name","");
                std::string version = prov.value("version","");
                std::vector<std::string> headers;
                if (prov.contains("headers") && prov["headers"].is_array())
                    for (auto& h : prov["headers"]) headers.push_back(h.get<std::string>());
                byName[name].emplace_back(version, mod.value("name",""), mpath, headers);
            }
        }
    }

    std::cout << "1. Checking vendored SDK headers...\n";
    for (auto& mod : ws["modules"]) {
        if (!mod.contains("path") || !mod.contains("name")) continue;
        std::string mname = mod.value("name","");
        std::string mpath = (modulesRoot_ / mod.value("path","")).string();
        fs::path sdkDir = fs::path(mpath) / "include" / "SDK";
        if (!fs::exists(sdkDir)) continue;

        for (const auto& unitDir : fs::directory_iterator(sdkDir)) {
            if (!unitDir.is_directory()) continue;
            std::string unitName = unitDir.path().filename().string();
            fs::path manFile = unitDir.path() / "SdkManifest.g.h";
            if (!fs::exists(manFile)) {
                std::cerr << "  WARN: " << mname << "/" << unitName << ": missing SdkManifest.g.h\n";
                allOk = false; continue;
            }
            // Parse version from manifest
            std::ifstream mf(manFile); std::string line;
            std::string version;
            while (std::getline(mf, line)) {
                if (line.find("_VERSION") != std::string::npos) {
                    size_t q1 = line.find('\"');
                    if (q1 != std::string::npos) {
                        size_t q2 = line.find('\"', q1+1);
                        if (q2 != std::string::npos) version = line.substr(q1+1, q2-q1-1);
                    }
                }
            }
            std::cout << "  " << mname << " " << unitName << "@" << version << ": ";
            // Verify against provider
            auto it = byName.find(unitName);
            if (it == byName.end()) {
                std::cout << "UNKNOWN PROVIDER\n";
                allOk = false; continue;
            }
            bool found = false;
            for (auto& v : it->second) {
                if (std::get<0>(v) == version) {
                    found = true;
                    std::string provMod = std::get<1>(v);
                    std::string provPath = std::get<2>(v);
                    auto& provHeaders = std::get<3>(v);
                    bool unitOk = true;
                    for (auto& hrel : provHeaders) {
                        fs::path src = fs::path(provPath) / hrel;
                        fs::path dst = unitDir.path() / fs::path(hrel).filename();
                        if (!fs::exists(src)) { std::cerr << "\n    MISSING canonical: " << src; unitOk = false; continue; }
                        if (!fs::exists(dst)) { std::cerr << "\n    MISSING vendored: " << dst; unitOk = false; continue; }
                        if (sha256_file(src) != sha256_file(dst)) {
                            std::cerr << "\n    DRIFT: " << fs::path(hrel).filename().string();
                            unitOk = false;
                        }
                    }
                    if (unitOk) std::cout << "OK (from " << provMod << ")";
                    else { allOk = false; }
                    break;
                }
            }
            if (!found) { std::cout << "VERSION MISMATCH\n"; allOk = false; }
            std::cout << "\n";
        }
    }

    std::cout << "\n2. Checking for stale duplicates in include/sdk/...\n";
    for (auto& mod : ws["modules"]) {
        if (!mod.contains("path") || !mod.contains("name")) continue;
        std::string mname = mod.value("name","");
        std::string mpath = (modulesRoot_ / mod.value("path","")).string();
        fs::path sdkDir = fs::path(mpath) / "include" / "sdk";
        if (!fs::exists(sdkDir) || !fs::is_directory(sdkDir)) continue;
        // Collect all SDK unit headers
        std::set<std::string> sdkHeaders;
        for (auto& [name, vers] : byName)
            for (auto& v : vers)
                for (auto& h : std::get<3>(v))
                    sdkHeaders.insert(fs::path(h).filename().string());
        for (const auto& entry : fs::directory_iterator(sdkDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".h") {
                std::string fname = entry.path().filename().string();
                if (sdkHeaders.find(fname) != sdkHeaders.end()) {
                    std::cout << "  WARN: " << mname << "/include/sdk/" << fname
                              << " may be a duplicate of a vendored SDK header\n";
                    allOk = false;
                }
            }
        }
    }

    if (allOk) {
        std::cout << "\n";
        cli::printSuccess("check-sdk PASSED");
    } else {
        std::cout << "\n";
        cli::printError("check-sdk FAILED (issues found)");
    }
}

// --- init-sdk: create SDK provider skeleton in a module ---
void Arche::doInitSdk(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Error: Usage: init-sdk <module> <unit-name> [version]\n";
        return;
    }
    std::string modName = args[0];
    std::string unitName = args[1];
    std::string version = (args.size() > 2) ? args[2] : "0.1.0";
    fs::path modPath = modulesRoot_ / modName;
    if (!fs::exists(modPath)) {
        std::cerr << "Error: ModuleInstance '" << modName << "' not found at " << modPath << "\n";
        return;
    }
    fs::path sdkDir = modPath / "include" / "SDK" / unitName;
    if (fs::exists(sdkDir)) {
        std::cerr << "Error: SDK unit '" << unitName << "' already exists in module '" << modName << "'\n";
        return;
    }
    try {
        fs::create_directories(sdkDir);
        // Create a template header
        std::string macroGuard = "SDK_" + macroIdent(unitName) + "_H";
        std::string headerPath = (sdkDir / (unitName + ".h")).string();
        std::ofstream hf(headerPath);
        hf << "#pragma once\n\n";
        hf << "#define " << macroGuard << "_VERSION \"" << version << "\"\n\n";
        hf << "// " << unitName << " SDK v" << version << "\n";
        hf << "// TODO: Add your SDK types and API here\n";
        hf.close();
        std::cout << "  created " << fs::path(headerPath).filename().string() << "\n";
        // Update or create configs/sdk-manifest.json
        fs::path manPath = modPath / "configs" / "sdk-manifest.json";
        fs::create_directories(manPath.parent_path());
        json manJson;
        if (fs::exists(manPath)) {
            std::ifstream mf(manPath); mf >> manJson;
        } else {
            manJson["provides"] = json::array();
            manJson["requires"] = json::array();
        }
        if (!manJson.contains("provides") || !manJson["provides"].is_array())
            manJson["provides"] = json::array();
        json provEntry;
        provEntry["name"] = unitName;
        provEntry["version"] = version;
        provEntry["headers"] = json::array({ "include/SDK/" + unitName + "/" + unitName + ".h" });
        manJson["provides"].push_back(provEntry);
        std::ofstream mf(manPath); mf << manJson.dump(4) << "\n";
        std::cout << "  updated configs/sdk-manifest.json\n";
        std::cout << "\nSDK unit '" << unitName << "@" << version << "' created in module '" << modName << "'.\n";
        std::cout << "  Next steps:\n";
        std::cout << "    1. Add header files to include/SDK/" << unitName << "/\n";
        std::cout << "    2. Update configs/sdk-manifest.json with the header list\n";
        std::cout << "    3. Run 'arche sync-sdk' to vendor headers into consumers\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// --- deps: show SDK dependency graph ---
void Arche::doDeps(const std::vector<std::string>& args) {
    fs::path wsJson = projectRoot_ / "configs" / "module_workspace.json";
    if (!fs::exists(wsJson)) { std::cerr << "error: module_workspace.json not found\n"; return; }
    std::ifstream wf(wsJson); json ws; wf >> ws;

    std::string filterMod = args.empty() ? "" : args[0];

    // Build provider registry
    std::map<std::string, std::vector<std::tuple<std::string,std::string,std::string,std::vector<std::string>>>> byName;
    std::map<std::string, std::vector<std::tuple<std::string,std::string,std::string,std::string,std::vector<std::string>>>> moduleProvides;
    for (auto& mod : ws["modules"]) {
        if (!mod.contains("path")) continue;
        std::string mname = mod.value("name","");
        std::string mpath = (modulesRoot_ / mod.value("path","")).string();
        fs::path mmanifest = fs::path(mpath) / "configs" / "sdk-manifest.json";
        if (!fs::exists(mmanifest)) continue;
        std::ifstream mf(mmanifest); json mj; mf >> mj;
        if (mj.contains("provides") && mj["provides"].is_array()) {
            for (auto& prov : mj["provides"]) {
                std::string name = prov.value("name","");
                std::string ver = prov.value("version","");
                std::vector<std::string> headers;
                if (prov.contains("headers") && prov["headers"].is_array())
                    for (auto& h : prov["headers"]) headers.push_back(h.get<std::string>());
                byName[name].emplace_back(ver, mname, mpath, headers);
                moduleProvides[mname].emplace_back(name, ver, mname, mpath, headers);
            }
        }
    }

    // Recursive printer
    std::function<void(const std::string&, const std::string&, const std::string&, int, std::set<std::string>&)> printTree;
    printTree = [&](const std::string& unit, const std::string& version, const std::string& indent, int depth, std::set<std::string>& seen) -> void {
        std::string key = unit + "@" + version;
        if (depth > 0) {
            if (seen.find(key) != seen.end()) {
                std::cout << indent << "└── " << unit << "@" << version << " (circular/duplicate)\n";
                return;
            }
            seen.insert(key);
        }
        if (depth > 0) std::cout << indent << "└── " << unit << "@" << version << "\n";
        // Find the provider's manifest to check requires
        auto it = byName.find(unit);
        if (it == byName.end()) return;
        for (auto& v : it->second) {
            if (std::get<0>(v) == version) {
                std::string provPath = std::get<2>(v);
                fs::path pman = fs::path(provPath) / "configs" / "sdk-manifest.json";
                if (fs::exists(pman)) {
                    std::ifstream pf(pman); json pj; pf >> pj;
                    if (pj.contains("requires") && pj["requires"].is_array()) {
                        auto& reqs = pj["requires"];
                        std::string childIndent = indent + (depth > 0 ? "    " : "");
                        for (size_t i = 0; i < reqs.size(); ++i) {
                            std::string rn = reqs[i].value("name","");
                            std::string rv = reqs[i].value("version","");
                            printTree(rn, rv, childIndent, depth + 1, seen);
                        }
                    }
                }
                break;
            }
        }
    };

    std::cout << "\nSDK Dependency Graph:\n";
    std::cout << "=====================\n\n";

    if (filterMod.empty()) {
        // Show all modules
        for (auto& [mname, provs] : moduleProvides) {
            std::cout << mname << " provides:\n";
            for (auto& p : provs) {
                std::set<std::string> seen;
                std::cout << "  " << std::get<0>(p) << "@" << std::get<1>(p) << "\n";
                // Find this unit's requires
                auto it = byName.find(std::get<0>(p));
                if (it != byName.end()) {
                    for (auto& v : it->second) {
                        if (std::get<0>(v) == std::get<1>(p)) {
                            std::string provPath = std::get<2>(v);
                            fs::path pman = fs::path(provPath) / "configs" / "sdk-manifest.json";
                            if (fs::exists(pman)) {
                                std::ifstream pf(pman); json pj; pf >> pj;
                                if (pj.contains("requires") && pj["requires"].is_array()) {
                                    for (auto& req : pj["requires"]) {
                                        std::string rn = req.value("name","");
                                        std::string rv = req.value("version","");
                                        printTree(rn, rv, "    ", 1, seen);
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
            std::cout << "\n";
        }
    } else {
        // Show specific module
        std::cout << filterMod << " provides:\n";
        auto it = moduleProvides.find(filterMod);
        if (it != moduleProvides.end()) {
            for (auto& p : it->second) {
                std::set<std::string> seen;
                std::cout << "  " << std::get<0>(p) << "@" << std::get<1>(p) << "\n";
                auto it2 = byName.find(std::get<0>(p));
                if (it2 != byName.end()) {
                    for (auto& v : it2->second) {
                        if (std::get<0>(v) == std::get<1>(p)) {
                            std::string provPath = std::get<2>(v);
                            fs::path pman = fs::path(provPath) / "configs" / "sdk-manifest.json";
                            if (fs::exists(pman)) {
                                std::ifstream pf(pman); json pj; pf >> pj;
                                if (pj.contains("requires") && pj["requires"].is_array()) {
                                    for (auto& req : pj["requires"]) {
                                        printTree(req.value("name",""), req.value("version",""), "    ", 1, seen);
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        } else {
            std::cout << "  (no SDK units provided or module not found)\n";
        }
        std::cout << "\n";
    }
    std::cout << "=====================\n";
}
