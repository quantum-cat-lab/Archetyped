#include "ClrHost.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include <hostfxr.h>
#include <coreclr_delegates.h>

ClrHost& ClrHost::instance() {
    static ClrHost inst;
    return inst;
}

bool ClrHost::ensureRuntimeConfig(std::string& outPath, const char* requested) {
    if (requested && requested[0]) {
        outPath = requested;
        return std::filesystem::exists(outPath);
    }
    outPath = "build/Archetyped.runtimeconfig.json";
    if (std::filesystem::exists(outPath)) return true;
    std::filesystem::create_directories("build");
    FILE* f = std::fopen(outPath.c_str(), "w");
    if (!f) return false;
    const char* json =
        "{\n"
        "  \"runtimeOptions\": {\n"
        "    \"tfm\": \"net9.0\",\n"
        "    \"framework\": { \"name\": \"Microsoft.NETCore.App\", \"version\": \"9.0.19\" }\n"
        "  }\n"
        "}\n";
    std::fwrite(json, 1, std::strlen(json), f);
    std::fclose(f);
    return true;
}

bool ClrHost::init(const char* runtimeConfigPath) {
    if (running_) return true;

    std::string cfg;
    if (!ensureRuntimeConfig(cfg, runtimeConfigPath)) {
        std::fprintf(stderr, "[ClrHost] runtimeconfig not found: %s\n", cfg.c_str());
        return false;
    }

    std::string hostfxrPath;
    const char* dotnetRoot = std::getenv("DOTNET_ROOT");
    if (dotnetRoot && dotnetRoot[0]) {
        hostfxrPath = std::string(dotnetRoot) + "/host/fxr/9.0.19/libhostfxr.so";
    } else {
        const char* home = std::getenv("HOME");
        if (home) hostfxrPath = std::string(home) + "/.dotnet/host/fxr/9.0.19/libhostfxr.so";
    }
    if (!std::filesystem::exists(hostfxrPath)) {
        hostfxrPath = "/usr/share/dotnet/host/fxr/9.0.19/libhostfxr.so";
    }

    libHostfxr_ = dlopen(hostfxrPath.c_str(), RTLD_NOW);
    if (!libHostfxr_) {
        std::fprintf(stderr, "[ClrHost] dlopen %s failed: %s\n", hostfxrPath.c_str(), dlerror());
        return false;
    }

    auto initFn = (hostfxr_initialize_for_runtime_config_fn)dlsym(libHostfxr_, "hostfxr_initialize_for_runtime_config");
    auto getDelFn = (hostfxr_get_runtime_delegate_fn)dlsym(libHostfxr_, "hostfxr_get_runtime_delegate");
    auto closeFn  = (hostfxr_close_fn)dlsym(libHostfxr_, "hostfxr_close");
    auto setErrFn = (hostfxr_set_error_writer_fn)dlsym(libHostfxr_, "hostfxr_set_error_writer");
    if (!initFn || !getDelFn || !closeFn) {
        std::fprintf(stderr, "[ClrHost] dlsym hostfxr failed\n");
        dlclose(libHostfxr_); libHostfxr_=nullptr;
        return false;
    }
        if(setErrFn){
        setErrFn([](const char* msg){ std::fprintf(stderr,"[hostfxr] %s\n", msg); });
    }

    hostfxr_handle ctx = nullptr;
    int rc = initFn(cfg.c_str(), nullptr, &ctx);
    if (rc != 0 || !ctx) {
        std::fprintf(stderr, "[ClrHost] hostfxr_initialize_for_runtime_config %d cfg=%s\n", rc, cfg.c_str());
        dlclose(libHostfxr_); libHostfxr_=nullptr;
        return false;
    }
    hostCtx_ = ctx;

    void* del = nullptr;
    rc = getDelFn(ctx, hdt_load_assembly_and_get_function_pointer, &del);
    if (rc != 0 || !del) {
        std::fprintf(stderr, "[ClrHost] get delegate failed %d\n", rc);
        closeFn(ctx); hostCtx_=nullptr;
        dlclose(libHostfxr_); libHostfxr_=nullptr;
        return false;
    }
    loadAssembly_ = (load_assembly_fn)del;
    {
        std::string bsPath;
        for (auto cand : {std::string("build/Bootstrap.dll"), std::string("build/ManagedBootstrap.dll"), std::string(std::string(getenv("HOME")?getenv("HOME"):"/home/mainmasgoose")+"/Documents/PROJECTS/Archetyped/build/Bootstrap.dll"), std::string(std::string(getenv("HOME")?getenv("HOME"):"/home/mainmasgoose")+"/Documents/PROJECTS/Archetyped/build/ManagedBootstrap.dll")}) {
            std::string abs = std::filesystem::absolute(cand).string();
            if (std::filesystem::exists(abs)) { bsPath = abs; break; }
            if (std::filesystem::exists(cand)) { bsPath = std::filesystem::absolute(cand).string(); break; }
        }
        if (bsPath.empty()) bsPath = std::filesystem::absolute("build/Bootstrap.dll").string();
        const char* bsDll = bsPath.c_str();
        void* func=nullptr; int rc2 = loadAssembly_(bsDll, "Bootstrap, Bootstrap", "CreateALC", UNMANAGEDCALLERSONLY_METHOD, nullptr, &func); std::fprintf(stderr, "[ClrHost] load CreateALC rc=%d func=%p\n", rc2, func); if (rc2 == 0 && func) bsCreate_ = (bootstrap_create_fn)func; func=nullptr; rc2 = loadAssembly_(bsDll, "Bootstrap, Bootstrap", "LoadAssembly", UNMANAGEDCALLERSONLY_METHOD, nullptr, &func); std::fprintf(stderr, "[ClrHost] load LoadAssembly rc=%d func=%p\n", rc2, func); if (rc2 == 0 && func) bsLoad_ = (bootstrap_load_fn)func; func=nullptr; rc2 = loadAssembly_(bsDll, "Bootstrap, Bootstrap", "GetFunctionPointer", UNMANAGEDCALLERSONLY_METHOD, nullptr, &func); std::fprintf(stderr, "[ClrHost] load GetFunctionPointer rc=%d func=%p\n", rc2, func); if (rc2 == 0 && func) bsGetFunc_ = (bootstrap_get_fn)func; func=nullptr; rc2 = loadAssembly_(bsDll, "Bootstrap, Bootstrap", "UnloadALC", UNMANAGEDCALLERSONLY_METHOD, nullptr, &func); std::fprintf(stderr, "[ClrHost] load UnloadALC rc=%d func=%p\n", rc2, func); if (rc2 == 0 && func) bsUnload_ = (bootstrap_unload_fn)func; func=nullptr; rc2 = loadAssembly_(bsDll, "Bootstrap, Bootstrap", "AddImport", UNMANAGEDCALLERSONLY_METHOD, nullptr, &func); std::fprintf(stderr, "[ClrHost] load AddImport rc=%d func=%p\n", rc2, func); if (rc2 == 0 && func) bsAddImport_ = (bootstrap_addImport_fn)func;

        std::fprintf(stderr, "[ClrHost] bootstrap %s create=%p load=%p get=%p unload=%p addImport=%p\n", bsDll, (void*)bsCreate_, (void*)bsLoad_, (void*)bsGetFunc_, (void*)bsUnload_, (void*)bsAddImport_);
    }
    running_ = true;
    std::fprintf(stderr, "[ClrHost] started cfg=%s hostfxr=%s\n", cfg.c_str(), hostfxrPath.c_str());
    return true;
}

void ClrHost::shutdown() {
    if (!running_) return;
    bsCreate_ = nullptr; bsLoad_ = nullptr; bsGetFunc_ = nullptr; bsUnload_ = nullptr; bsAddImport_ = nullptr;
    if (libHostfxr_ && hostCtx_) {
        auto closeFn = (hostfxr_close_fn)dlsym(libHostfxr_, "hostfxr_close");
        if (closeFn) closeFn((hostfxr_handle)hostCtx_);
    }
    hostCtx_ = nullptr;
    loadAssembly_ = nullptr;
    if (libHostfxr_) { dlclose(libHostfxr_); libHostfxr_=nullptr; }
    running_ = false;
    std::fprintf(stderr, "[ClrHost] shutdown\n");
}
