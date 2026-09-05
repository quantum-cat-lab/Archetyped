# Module template

`forge mod new <name>` copies `assets/templates/module/` into a new
directory and substitutes `{{MODULE_NAME}}` everywhere. This page is a
walkthrough of what you get and what each piece is for.

## What you get

```
my-game/                                      ← the new module
├── CMakeLists.txt                            ← SHARED lib, .so/.dll by name
├── Arche.my-game.csproj                      ← (--csharp only)
├── README.md
├── .gitignore                                ← ignores build/, .cache/
├── .gitattributes
├── module.json                               ← metadata (forge-generated)
├── assets/                                   ← copied to container/<c>/assets/
├── configs/
│   └── sdk-manifest.json                     ← module's view of SDK deps
├── include/
│   └── SDK/                                  ← vendored by forge sdk sync
├── src/
│   ├── module.cpp                            ← the native entry
│   ├── main/
│   │   └── csharp/                           ← (--csharp only)
│   │       └── Arche.my-game/
│   │           └── Entry.cs
│   └── headers/
│       ├── Engine.hpp                        ← convenience facade
│       └── docs/                             ← SDK doc snippets
└── scripts/
    ├── build.sh                              ← convenience wrapper
    ├── build-improved.sh                     ← variant with -j$(nproc)
    └── build-windows*.bat
```

## `CMakeLists.txt`

Three things matter:

1. **It's a SHARED library, not an executable.** The engine loads it
   with `dlopen`, so `add_library(my-game SHARED …)`.

2. **Sources are globbed, not listed.** `file(GLOB_RECURSE …)` collects
   everything under `src/`. Adding a new `.cpp` doesn't require editing
   CMake.

3. **Output name has no `lib` prefix on Unix.** `OUTPUT_NAME "my-game"`
   produces `my-game.so` (not `libmy-game.so`), matching what the engine
   loader expects.

```cmake
cmake_minimum_required(VERSION 3.5)
project(my-game LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(my-game SHARED ${MODULE_SOURCES})
target_include_directories(my-game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(my-game PROPERTIES OUTPUT_NAME "my-game")
if(UNIX)
    set_target_properties(my-game PROPERTIES PREFIX "")
endif()
```

`WINDOWS_EXPORT_ALL_SYMBOLS` is also set on Windows so the DLL doesn't
need a `.def` file.

## `src/module.cpp` — the native entry

```cpp
#include <SDK/archetyped/FractalSDK.h>
#include <SDK/archetyped/hash/hash.h>
#include <SDK/archetyped/IKernel.h>
#include <SDK/archetyped/ECS.h>
#include <iostream>

#ifdef _WIN32
    #define FRACTAL_EXPORT extern "C" __declspec(dllexport)
#else
    #define FRACTAL_EXPORT extern "C" __attribute__((visibility("default")))
#endif

FRACTAL_EXPORT void ModuleMain(IKernel* kernel) {
    FractalSDK::SDK::Initialize(kernel);

    std::cout << "[ModuleInstance] Initialized successfully." << std::endl;

    // TODO: Implement module logic here
}
```

`ModuleMain` is the only symbol the engine requires. It runs once,
synchronously, on the engine's main thread during `dlopen`. Three things
to know:

- `FRACTAL_EXPORT` — forces the symbol to be visible on all platforms.
  Without it, `--gc-sections` or `__declspec` defaults will strip it.
- `FractalSDK::SDK::Initialize(kernel)` — hands the `IKernel*` to a
  thread-local singleton. All SDK wrappers (`ECS`, `EventInstance`,
  `WorkScheduler`, …) read from this singleton, so you don't pass
  `kernel` around manually.
- Return immediately if you can. Long init in `ModuleMain` blocks the
  engine's startup. Use a `SmartScheduler::scheduleDelayed` call to do
  real work after init.

## `Arche.my-game.csproj` — the C# project

Created only with `--csharp` or `--csharp-only`. `<EnableDynamicLoading>`
is on so the DLL is loaded into its own ALC; `<AllowUnsafeBlocks>` is
on so the SDK can use `IntPtr` plumbing without ceremony.

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net9.0</TargetFramework>
    <EnableDynamicLoading>true</EnableDynamicLoading>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <AssemblyName>Arche.my-game</AssemblyName>
    <RootNamespace>Arche.my-game</RootNamespace>
  </PropertyGroup>
</Project>
```

Build with `dotnet publish` (which `forge cont build` does for you):

```bash
dotnet publish Arche.my-game.csproj -c Release -o container/test/bin
```

## `src/main/csharp/Arche.my-game/Entry.cs` — the C# entry

```csharp
using System;
using System.Runtime.InteropServices;

namespace Arche.my-game;

public static class Entry
{
    [UnmanagedCallersOnly(EntryPoint = "Init")]
    public static void Init()
    {
        Console.WriteLine("[Arche.my-game] Init");
    }

    [UnmanagedCallersOnly(EntryPoint = "Tick")]
    public static void Tick(float dt) { }

    [UnmanagedCallersOnly(EntryPoint = "Shutdown")]
    public static void Shutdown()
    {
        Console.WriteLine("[Arche.my-game] Shutdown");
    }
}
```

`[UnmanagedCallersOnly]` exports the method as a C-callable function
pointer with no marshalling overhead. The engine calls `Init` once on
load, `Tick` every frame, `Shutdown` on unload. The names are convention;
the engine wires them up because it knows the module's namespace
(`Arche.<MODULE_NAME>`).

## `module.json` — the metadata

`forge mod new` writes this from the `{{MODULE_NAME}}` substitution and
hardcoded defaults:

```json
{
  "id": "my-game",
  "name": "my-game",
  "version": "1.0.0",
  "assets_root":  "assets/",
  "config_root":  "configs/",
  "dependencies": [],
  "entry_point":  "my-game.so"
}
```

Edit `version` when you ship a release. Add entries to `dependencies` if
your module calls SDK methods that require other modules to be loaded
first (rare — most cross-module calls are just packet sends).

## `configs/sdk-manifest.json` — module-side SDK view

```json
{
  "provides": [],
  "requires": [
    { "name": "archetyped", "version": "1.0.0" }
  ]
}
```

`requires` is what the module wants from upstream SDK providers. The
matching `provides` lives in the engine's `configs/sdk-manifest.json`
(under `archetyped`). `forge sdk check` verifies that what the engine
provides matches what the module requires.

## `include/SDK/` — vendored headers

`forge sdk sync` writes the engine's `include/SDK/archetyped/*` here.
After running `forge mod new`, the directory exists but is empty until
you run sync:

```bash
forge sdk sync
```

This is the only way to get the SDK into the module — copying by hand
will drift.

## `src/headers/Engine.hpp` — convenience facade

A one-file shim that forward-declares the SDK entry points so you can
include a single header in deeply-nested code. Most modules ignore it
and just `#include <SDK/archetyped/...>` directly.

## `scripts/`

Three convenience wrappers, all calling the same `cmake -S . -B build
&& cmake --build build`:

- `build.sh` — POSIX `nproc`-aware parallel build.
- `build-improved.sh` — same with stricter warnings.
- `build-windows*.bat` — MSVC / MinGW variants for cross-compiling.

You usually don't run these — `forge cont build` does it for you. They
exist for stand-alone module development without a container.

## `assets/` and `configs/`

Anything under `assets/` is copied verbatim into
`container/<container>/assets/<module-id>/` at build time. Same for
`configs/`. Use them for sprites, scripts, level data, and
container-specific overrides.

## `.gitignore`

The template ships a minimal one:

```
/.cache
/build
```

Add more as you need ( `.vs/`, `.idea/`, `*.user` ).

## Putting it all together

```bash
# 1. Create
forge mod new my-game --csharp

# 2. Edit src/module.cpp + Entry.cs
$EDITOR my-game/src/module.cpp
$EDITOR my-game/src/main/csharp/Arche.my-game/Entry.cs

# 3. Vendor the SDK
forge sdk sync

# 4. Add to a container
forge cont add test my-game

# 5. Build
forge cont build test

# 6. Run
forge cont run test
```

That's a complete module. Everything after step 5 is normal engine work —
your module runs in the same way any of the bundled ones does.
