# Archetyped

Cool C++ Game :3

A modular C++23 game engine core. Service-oriented, ABI-stable,
multi-runtime. Modules (C++/Kotlin/C#) talk to the engine through the
**FUR** command router — no virtuals on hot paths, no shared globals, no
Lua. Build your game as a set of typed services and swap runtimes
per-module.

## Get started

```bash
# 1. Clone alongside your modules
git clone <this-repo>                       # → ./Archetyped
git clone <your-game-modules> ../           # → ./game-modules/

# 2. Initialize workspace configs
cd Archetyped
./build/forge init                          # idempotent

# 3. Build the engine
./build/forge engine-build

# 4. Add a module and build a container
./build/forge mod new my-game --csharp
./build/forge cont add test my-game
./build/forge cont build test

# 5. Run
./build/forge cont run test
```

You only need a C++23 compiler, CMake, and (for C# / Kotlin modules)
the `dotnet` SDK and/or a JDK. No Lua, no Python, no extra runtime.

## Workspace layout

```
PROJECTS/                                   ← workspace root
├── Archetyped/                             ← this repo
│   ├── CMakeLists.txt
│   ├── build/                              ← engine + forge binaries
│   ├── configs/                            ← forge-managed state
│   ├── container/                          ← built container images
│   ├── include/SDK/archetyped/             ← public SDK headers
│   ├── src/
│   │   ├── engine/                         ← core (components/, FUR/, services/)
│   │   ├── main/csharp/Bootstrap/          ← .NET ALC host
│   │   └── tools/forge/                    ← CLI tool
│   ├── assets/templates/module/            ← `forge mod new` template
│   └── docs/                               ← detailed documentation
│
├── my-game/                                ← your modules (siblings)
├── my-tools/
└── ...
```

## Documentation

- [**`docs/forge.md`**](docs/forge.md) — `forge` CLI reference: every
  command, flag, and configuration file.
- [**`docs/modules.md`**](docs/modules.md) — module authoring:
  `module.json`, native + C# entry points, lifecycle, hot reload.
- [**`docs/module-template.md`**](docs/module-template.md) — what
  `forge mod new` actually generates: CMakeLists, module.cpp,
  Entry.cs, the SDK layout.
- [**`docs/sdk.md`**](docs/sdk.md) — SDK layout, the hash protocol,
  payload conventions, ABI rules.
- [**`docs/architecture.md`**](docs/architecture.md) — engine internals:
  FUR routing, service tiers (Host / Factory / Instance / Store),
  runtimes, memory model.

## Status

Alpha / dev. Core engine + service model stable; the editor/UX is
minimal (CLI only). See `docs/` for what works and how.

## License

See `LICENSE`.
