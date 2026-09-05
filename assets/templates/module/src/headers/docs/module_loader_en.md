# Archetyped: Module Loader

The Module Loader service allows the engine to load dynamic libraries (DLLs/Shared Objects) at runtime.

## Overview

Modules are external libraries that can register new services or extend existing ones. Each module should have an entry point that receives the `KernelAPI`.

## Module Entry Point

A module must export a function compatible with `ModuleEntry`:

```cpp
extern "C" void FractalModuleEntry(KernelAPI api) {
    // Register custom FUR methods here
    // api.registerCMDMethod(...);
}
```

## Loading Modules via SDK

### FUR Methods

-   `loadModuleHash`: Loads a module from the specified file path.
-   `isLoadedHash`: Checks if a module is already loaded.

### Example

```cpp
loadModuleContext ctx;
ctx.path = "path/to/my_module.so";

FURCMDPacket packet;
packet.methodHash = loadModuleHash;
packet.payloadSize = sizeof(ctx);
packet.payload = &ctx;

FractalSDK::SDK::Get()->sendPacket(packet);
```
