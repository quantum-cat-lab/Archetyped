# Archetyped

Cool C++ Game :3

A modular C++23 game engine core. Service-oriented, ABI-stable, multi-runtime.
Modules (C++/Kotlin/C#) talk to the engine through the **FUR** command router —
no virtuals on hot paths, no shared globals, no Lua. Build your game as a set
of typed services and swap runtimes per-module.

## Quickstart

```bash
# 1. Clone alongside your modules (see Workspace layout below)
git clone <this-repo>                       # → ./Archetyped
git clone <your-game-modules> ../           # → ./game-modules/

# 2. Initialize workspace configs
cd Archetyped
./build/forge init                          # idempotent: safe to re-run

# 3. Build the engine core
./build/forge engine-build

# 4. Build a container (groups modules into a runnable image)
./build/forge cont build test

# 5. Run
./build/forge cont run test
# or directly:
./build/archetyped
```

`forge` is a small CLI inside the repo (`src/tools/forge/`) that wraps CMake,
SDK syncing, and the module container workflow. After a one-time `./build/forge
engine-build`, you can `ln -s build/forge ~/.local/bin/forge` to call it
globally.

## Workspace layout

```
PROJECTS/                                   ← Workspace root (Workspace.modulesRoot)
├── Archetyped/                             ← this repo
│   ├── CMakeLists.txt
│   ├── build/                              ← engine + forge binary
│   ├── configs/                            ← forge-managed state
│   │   ├── active_container.json           ← default container
│   │   ├── module_workspace.json           ← discovered modules + systems
│   │   └── sdk-manifest.json
│   ├── container/                          ← built container images
│   │   └── test/
│   │       ├── bin/                        ← *.so + .dll
│   │       ├── assets/
│   │       └── configs/
│   ├── include/SDK/archetyped/             ← public SDK headers
│   ├── src/
│   │   ├── engine/                         ← core (components/, FUR/, services/)
│   │   ├── main/csharp/Bootstrap/          ← ALC host for .NET
│   │   └── tools/forge/                    ← CLI tool
│   └── assets/templates/module/            ← `forge mod new` template
│
├── RealmX/                                 ← your game modules (siblings)
├── AtlasX/
├── TetherX/
└── ...
```

Modules are discovered by walking the directory **above** the project root.
Adding a new module = drop a folder with `module.json` next to `Archetyped/`,
then re-run `forge init`.

## Build

The project is plain CMake. Forge is a thin wrapper.

```bash
mkdir -p build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel                   # builds engine + forge
./forge engine-build                         # also publishes ManagedBootstrap
./forge cont build test                      # builds the 'test' container
```

Options:
- `ENABLE_JVM=ON/OFF` — build with JNI host (default ON)
- `ENABLE_CLR=ON/OFF` — build with .NET host (default OFF; needs `dotnet` SDK)

## `forge` CLI

`forge` is the in-tree CLI. Top-level verbs:

| Verb | Purpose |
|---|---|
| `forge init [--reset]` | Initialize `configs/` (idempotent) |
| `forge engine-build` | Build engine + publish ManagedBootstrap |
| `forge forge-build` | Rebuild only the `forge` tool |
| `forge cont new <name>` | Create a new container |
| `forge cont add <c> <m>` | Add module `m` to container `c` |
| `forge cont build <name>` | Build a container |
| `forge cont run <name>` | Run a container |
| `forge mod new <name> [--csharp]` | Create a new module from template |
| `forge mod list` | List discovered modules |
| `forge sdk sync` | Vendor SDK headers to all modules |
| `forge sdk check` | Verify SDK integrity |
| `forge doctor` | System diagnostics |
| `forge status` | Project status dashboard |
| `forge help [cmd]` | Show help |

Noun-group syntax is supported: `forge cont build test` ⇔ `forge build test`.

## Module authoring

A module is a CMake project that depends on `archetyped` SDK headers. The
SDK is vendored by `forge sdk sync` — you don't manage it.

```bash
forge mod new my-game --csharp             # C++ + C# (hybrid)
forge mod new my-tools --csharp-only       # pure C# module
```

The template ( `assets/templates/module/` ) gives you:
- `CMakeLists.txt` that links against `archetyped` SDK
- `src/module.cpp` with the `ModuleMain` entry point
- `module.json` with metadata
- (with `--csharp`) `Arche.<name>.csproj` + `src/main/csharp/Arche.<name>/Entry.cs`

To talk to the engine, include the SDK and use the typed C++ wrappers:
```cpp
#include <SDK/archetyped/ECS.h>
#include <SDK/archetyped/EventInstance.h>

ECS ecs("MyGame");
EventInstance bus("MyGame");

ecs.registerComponent<Position>(fnv1aHashConst("position"), 1024);
bus.subscribe(fnv1aHashConst("OnPickup"), onPickupCallback);
```

## Architecture

### Command routing (FUR)

The engine exposes no virtual interface. Every service is reached through a
**FURCMDPacket** — a `{methodHash, payload, payloadSize, outputBuffer, fence}`
POD struct. The hash is a compile-time FNV-1a of a string like
`"archetyped:ecs:registerComponent"`. The engine routes the packet to a
handler on a worker thread, fills the output buffer, and signals the fence.

```text
[ Module ]
   │  constructs FURCMDPacket in its own memory
   ▼
[ SDK wrapper ] (inline fn, generates packet)
   │  sendPacket(packet)
   ▼
[ Kernel ]  ── buffers ──>  [ Worker thread ]  ──>  [ Service handler ]
   ▲                                                  │
   └────── fence/atomic ─────────────────────────────┘
```

This gives modules a stable ABI (the C struct never changes), bounded
latency (single-producer/single-consumer per packet), and an obvious injection
point for tracing.

### Service model

Engine services are split into tiers, each with a distinct responsibility and
naming convention:

- **Tier 1 — Singletons** (one per kernel, suffix `Host` / `Scheduler`):
  `ClrHost`, `JvmHost`, `SmartScheduler`, `VFS`, `Clock`, `BridgeRegistry`.
- **Tier 2 — Factories** (one per kernel, owns a map of per-domain instances,
  suffix `Factory`): `ClrFactory`, `JvmFactory`, `CSharpFactory`,
  `KotlinFactory`, `SchemaFactory`, `CMFactory`, `EBFactory`, `WSFactory`,
  `SQLFactory`, `ModuleFactory`.
- **Tier 3 — Instances** (per-domain POJO, suffix `Instance`): `ClrInstance`,
  `JvmInstance`, `SqlInstance`, `EventInstance`, `VaultInstance`, `ComponentInstance`,
  `ModuleInstance`, `SchemaInstance`.
- **Tier 4 — Per-kernel stores** (one per kernel, internal only): `MutexStore`,
  `SubmoduleRegistry`.

Each call flows Tier 2 → Tier 3. The factory owns the lifecycle
(`createInstance` / `destroyInstance`); the instance owns the per-domain state.

### Module contract

A module is a `.so` (or `.dll`) that exports a `ModuleMain` function:

```cpp
extern "C" void ModuleMain(IKernel* kernel, ModuleConfig config);
```

`IKernel` exposes `sendPacket` and `registerCMDMethod`. Modules never see
engine-internal types directly — only SDK headers in `include/SDK/archetyped/`.

### Runtimes

The engine itself is C++23. Modules can additionally ship code in:

- **C#** — `ClrHost` loads a CoreCLR ALC, resolved through
  `src/main/csharp/Bootstrap/Bootstrap.cs` (unified with module layout at
  `src/main/csharp/`). `KotlinRuntime` uses `JvmHost` for `.jar`/JVM
  embedding.
- **Lua** is intentionally not supported. Use C# or Kotlin for scripting.

### Containers

A **container** is a runnable image: a list of modules + their order +
their `assets/` and `configs/`. `forge cont build <name>` produces
`container/<name>/bin/` (the `.so`s + `.dll`s) and copies the assets. The
engine is loaded once and the modules dynamically `dlopen`'d.

## Status

Alpha / dev. No external dependencies required for the core (C++ + CMake +
optional `dotnet` + optional JDK). Vulkan rendering is in `AmethystX`. Voxel
world in `AtlasX`/`RealmX`.

## License

See `LICENSE`.
