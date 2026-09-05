# Modules

A **module** is a `.so` (or `.dll`) that the engine loads at runtime. It
exports a single entry point and talks to the engine through the SDK
headers — never directly.

## What a module looks like on disk

```
my-game/
├── CMakeLists.txt
├── module.json
├── Arche.my-game.csproj           (if --csharp)
├── assets/
│   └── …                          (copied into container/<c>/assets/my-game/)
├── configs/
│   └── …                          (copied into container/<c>/configs/my-game/)
├── include/
│   └── SDK/archetyped/            (vendored by forge sdk sync)
└── src/
    ├── module.cpp                 (the native entry)
    └── main/
        └── csharp/                (C# sources, same layout as the engine)
            └── Arche.my-game/
                └── Entry.cs
```

## `module.json`

Every module has a `module.json` at its root. `forge` uses it to discover
the module and pick up its entry point.

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

- `id` — used by `forge mod new`, `forge cont add`, and SDK dependencies.
- `entry_point` — the `.so` / `.dll` filename. Defaults to `<id>.so` if
  omitted.
- `dependencies` — other module ids this module needs. Used for load-order
  resolution.
- `assets_root` / `config_root` — relative to module root, copied into
  the container at build time.

## Native entry point

`src/module.cpp` exports `ModuleMain`:

```cpp
#include <SDK/archetyped/IKernel.h>
#include <SDK/archetyped/FractalSDK.h>

extern "C" void ModuleMain(IKernel* kernel, ModuleConfig config) {
    // Called once on dlopen. Register your systems, claim your domain.
    auto& sdk = FractalSDK::SDK::Get();

    // Subscribe to an event
    sdk.registerEvent("OnTick", [](FURCMDPacket*) { /* … */ });

    // Or use the typed wrappers
    ECS ecs("my-game");
    ecs.registerComponent<Position>(fnv1aHashConst("position"), 1024);
}
```

`IKernel` exposes two things:

- `sendPacket(FURCMDPacket&)` — fire a command to the engine.
- `registerCMDMethod(uint32_t hash, void* fn)` — let the engine call back
  into the module by hash.

`ModuleConfig` carries a small bag of strings (argv-style) and a userdata
pointer. The convention is undocumented-by-design: each module's
`ModuleMain` parses what it needs.

## C# hybrid modules

`forge mod new my-game --csharp` adds:

- `Arche.my-game.csproj` — `<EnableDynamicLoading>true</EnableDynamicLoading>`,
  references `archetyped` SDK through the .NET side (`CLR.h`).
- `src/main/csharp/Arche.my-game/Entry.cs` — entry with `Init` / `Tick` /
  `Shutdown` exported via `[UnmanagedCallersOnly]`.

The C# DLL is loaded by `ClrHost` (hostfxr + CoreCLR) into a collectible
ALC managed by `src/main/csharp/Bootstrap/Bootstrap.cs` in the engine. The
module's domain is named after the module id; calls between C++ and C#
happen through the same `FURCMDPacket` mechanism, only wrapped in
managed delegates.

Build a hybrid module:

```bash
forge mod new my-game --csharp
forge mod add test my-game         # adds to default stages
forge cont build test              # builds .so + .dll
```

## Lifecycle

1. Engine starts, runs `Bootstrap.init`, then opens the active container.
2. For each module in `stage0_boot` → `preload` → `load_order` →
   `stage1_hosts` → `stage2_submodules` → `stage3_finalize`:
   - `dlopen(module.so)`.
   - Call `ModuleMain(kernel, config)`.
3. Engine enters the main loop: a `FURCMD` worker dispatches engine
   services; a `frameTick` packet drives module `update` callbacks.
4. On shutdown: reverse order, then `kernel.shutdown`.

Module unload is a no-op for the engine — once `dlclose`'d, the module's
state is gone. State that should survive (component tables, event
registrations) lives in the engine's per-domain registries (Tier-2
factories). See [`architecture.md`](architecture.md).

## Adding a new module

```bash
forge mod new my-game                       # C++ only
forge mod new my-game --csharp              # C++ + C# hybrid
forge mod new my-pure-cs --csharp-only      # C# only (no native build)
```

`forge mod new` will:

1. Copy `assets/templates/module/` into `<workspace>/<id>/`.
2. Substitute `{{MODULE_NAME}}` with the new id.
3. Write `module.json`.
4. With `--csharp`: write `Arche.<id>.csproj` and `src/main/csharp/Arche.<id>/Entry.cs`.
5. Register the module in `configs/module_workspace.json` (id + path).

Then:

```bash
forge cont add <container> my-game          # add to a container
forge sdk sync                              # vendor SDK into the new module
forge cont build <container>                # rebuild container
```

## Maintaining a module

- **Re-run the template** when the engine template changes:
  `forge mod update my-game` re-applies the template non-destructively
  (never overwrites `src/`).

- **Refresh the SDK** when the engine's public headers change:
  `forge sdk sync` rewrites `include/SDK/archetyped/` in every module.
  `forge sdk check` reports drift.

- **Hot reload during dev**: the engine watches the module's `build/`
  directory and re-`dlopen`s on rebuild. `forge watch <container>` runs
  the engine with watch enabled.

- **Module self-deletion**: just `rm -rf` the module directory. Then
  `forge init` to refresh `module_workspace.json`.

## Multi-process / submodules

A `submodule` is a child module loaded by another module. The host
module implements `ISubmoduleHost` (in `include/SDK/archetyped/`) and the
engine routes `forge cont add` entries in `stage2_submodules` to the
right host. The submodules are regular modules — same `module.json`,
same `ModuleMain` — they just live under a host's lifecycle.

```json
{
  "stages": {
    "stage2_submodules": {
      "MyHostModule": ["sub-a", "sub-b"]
    }
  }
}
```

## Module conventions

- One `.so` per module. Don't link other modules statically — use
  the SDK to talk to them.
- Module code may not link against engine internals (`src/engine/`
  outside `include/SDK/`). The SDK is the contract.
- Avoid blocking the calling thread. Engine packets are dispatched on
  workers; if you need work done off the main thread, use the
  `WorkScheduler` (`WSFactory`) from the SDK.
- Persist no global state outside the engine's registries. Per-domain
  state goes in a Tier-2 factory; per-kernel state is not allowed.
