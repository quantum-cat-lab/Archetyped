#pragma once
#include <string>

class ClrHost {
public:
    static ClrHost& instance();
    bool init(const char* runtimeConfigPath = nullptr);
    void shutdown();
    bool isRunning() const { return running_; }

    using load_assembly_fn = int(*)(const char*, const char*, const char*, const char*, void*, void**);
    load_assembly_fn getLoadAssemblyFn() const { return loadAssembly_; }

    using bootstrap_create_fn = int(*)(const char*);
    using bootstrap_load_fn = int(*)(const char*, const char*);
    using bootstrap_get_fn = void*(*)(const char*, const char*, const char*);
    using bootstrap_unload_fn = int(*)(const char*);
    using bootstrap_addImport_fn = int(*)(const char*, const char*);

    bootstrap_create_fn getBootstrapCreate() const { return bsCreate_; }
    bootstrap_load_fn getBootstrapLoad() const { return bsLoad_; }
    bootstrap_get_fn getBootstrapGetFunc() const { return bsGetFunc_; }
    bootstrap_unload_fn getBootstrapUnload() const { return bsUnload_; }
    bootstrap_addImport_fn getBootstrapAddImport() const { return bsAddImport_; }

    void* hostContext() const { return hostCtx_; }

private:
    ClrHost() = default;
    ~ClrHost() { shutdown(); }
    ClrHost(const ClrHost&) = delete;
    ClrHost& operator=(const ClrHost&) = delete;

    bool ensureRuntimeConfig(std::string& outPath, const char* requested);

    void* libHostfxr_ = nullptr;
    void* hostCtx_ = nullptr;
    load_assembly_fn loadAssembly_ = nullptr;
    bootstrap_create_fn bsCreate_ = nullptr;
    bootstrap_load_fn bsLoad_ = nullptr;
    bootstrap_get_fn bsGetFunc_ = nullptr;
    bootstrap_unload_fn bsUnload_ = nullptr;
    bootstrap_addImport_fn bsAddImport_ = nullptr;
    bool running_ = false;
};
