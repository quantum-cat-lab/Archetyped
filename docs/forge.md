# forge CLI

`forge` is the in-repo toolchain wrapper. It is built together with the engine
(`src/tools/forge/`) and lives at `build/forge`. After the first
`engine-build` you can symlink it onto your PATH:

```bash
ln -s "$(pwd)/build/forge" ~/.local/bin/forge
```

`forge` has three jobs:

1. **Build orchestration** — wraps CMake, publishes the managed bootstrap
   (.NET ALC host), copies artifacts into containers.
2. **SDK distribution** — vendors public headers from
   `include/SDK/archetyped/` into every module under `../`.
3. **Workspace inspection** — reads `configs/`, scans sibling modules,
   reports health.

## Top-level commands

| Command | What it does |
|---|---|
| `forge init [--reset]` | Initialize `configs/`. Idempotent. See below. |
| `forge engine-build [--recache]` | Build engine + publish ManagedBootstrap. |
| `forge forge-build` | Rebuild only the `forge` binary. |
| `forge clean [--all]` | Delete `build/` and module `build/` dirs. |
| `forge doctor [--quick]` | System diagnostics (compiler, cmake, modules, CPU). |
| `forge status` | Project status dashboard. |
| `forge help [cmd]` | Show top-level or per-command help. |
| `forge completions <shell>` | Emit `bash` / `zsh` / `fish` completion script. |

## `forge init`

Scans the current project root and materializes the workspace state.

```bash
forge init                  # creates configs/{active,workspace,sdk}.json
forge init --reset          # overwrite active_container.json + sdk-manifest.json
```

It writes (or reuses):

- `configs/active_container.json` — `{ "active_container": "test" }`
- `configs/module_workspace.json` — `{ modules: [...], enabled_systems: [...] }`
  - Walks `..` (the workspace parent) and lists every directory with a
    `module.json`.
  - Walks `src/engine/components/` and lists every directory whose name
    matches the known system set (ECS, Event, DataBase, …).
- `configs/sdk-manifest.json` — empty `provides/requires` skeleton if missing.

Re-running is safe: `active_container.json` and `sdk-manifest.json` are kept
unless `--reset`. `module_workspace.json` is regenerated every time so the
scan always reflects the filesystem.

## Container workflow (`forge cont ...`)

A **container** is a runnable image: an ordered list of modules + their
`assets/` and `configs/`. `forge cont build <name>` produces
`container/<name>/bin/` (the `.so`s + `.dll`s) and copies the assets.

```bash
forge cont new test                      # create container 'test' skeleton
forge cont add test RealmX               # add module to default stages
forge cont add test AtlasX --stage=stage0_boot
forge cont info test                     # dump container.json
forge cont build test                    # build (incremental, hash-cached)
forge cont run test                      # launch
forge cont rm test AtlasX                # remove module
forge cont cp test game                  # clone container
forge cont archive test                  # → .tar.zst
forge cont unarchive ./test.tar.zst      # restore
forge cont delete test                   # rm -rf container/test
```

Stages a container understands (in load order):
- `stage0_boot` — modules that must load before the engine finishes init
- `preload` — pulled in before user modules
- `load_order` — main user modules
- `stage1_hosts` — host loaders (Clr, Jvm) — usually already in preload
- `stage2_submodules` — `{ hostId: [submoduleId, ...] }` — modules that
  attach to a host (e.g. multiple game modules under one ClrHost)
- `stage3_finalize` — modules that need everything else loaded first

`forge cont add` defaults to `preload` if `--stage` is omitted.

## Module workflow (`forge mod ...`)

```bash
forge mod new my-game                    # C++ module from template
forge mod new my-tools --csharp          # C++ + C# hybrid
forge mod new my-pure-cs --csharp-only   # pure C# module (no native build)
forge mod list                           # list discovered modules
forge mod info RealmX                    # show module.json + path
forge mod update my-game                 # re-run template (non-destructive)
forge mod scan                           # refresh module_workspace.json
```

`forge mod new` copies `assets/templates/module/` and substitutes
`{{MODULE_NAME}}` everywhere. With `--csharp` it also creates
`Arche.<name>.csproj` + `src/main/csharp/Arche.<name>/Entry.cs`.

## SDK workflow (`forge sdk ...`)

```bash
forge sdk sync                           # vendor SDK headers to all modules
forge sdk check                          # verify module SDK matches source
forge sdk deps [module]                  # dependency graph
```

`sdk sync` reads `configs/sdk-manifest.json` and writes the listed files into
each sibling module's `include/SDK/archetyped/`. Pass `--force` to overwrite
files that already exist; pass `--module <name>` to vendor into one module
only.

`forge sdk check` SHA-256-hashes every vendored header in every module
against the hash recorded at sync time. A mismatch means a module drifted
from the engine — run `forge sdk sync` to fix.

## Build

`forge engine-build` runs:

1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
2. `cmake --build build --parallel`
3. If the engine built OK, `dotnet publish src/main/csharp/Bootstrap -c Release
   -o build` and renames `Bootstrap.dll` → `ManagedBootstrap.dll` for
   `ClrHost` compatibility.
4. Copies `include/Engine/*.hpp` → `assets/templates/module/src/headers/`
   so the module template always matches.

The `--recache` flag nukes `build/` first.

## Caching

Module builds are cached by content hash. `forge cont build` stores
`.build_cache/.forge_src_hash` in each module; if the source files
(`*.cpp`/`*.h`/`*.hpp`/`module.json`/`CMakeLists.txt`) haven't changed, the
module is skipped.

`--force` (or `-f`) bypasses the cache for one run.

## REPL

`forge` without arguments starts a small REPL with history and arrow keys.
Inside the REPL, `use <container>` sets a persistent default — subsequent
`build` / `run` / `info` commands will use it without an explicit argument.
`use ..` clears.

## Configuration files

| File | Owned by | Purpose |
|---|---|---|
| `configs/active_container.json` | user (forged by `init`) | default container for `cont build` / `run` |
| `configs/module_workspace.json` | `forge init` (regenerated) | discovered modules + enabled systems |
| `configs/sdk-manifest.json` | user (extended by `sdk init`) | SDK provider list (one per source) |
| `container/<name>/container.json` | user (extended by `cont add`) | container definition |
| `module.json` | `forge mod new` | module metadata |
