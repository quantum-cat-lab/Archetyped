# Archetyped

A high-performance, modular C++23 engine built on the principles of **Command Routing** and **ABI Stability**.

## 🚀 Engine Architecture

Archetyped is designed to be highly decoupled. Communication between modules and core services is handled by the **Fast Utility Router (FUR)**.

```text
[ External Modules ] <--- [ SDK Headers ]
      |
      v
[ FUR Command Router ] (Buffered / Asynchronous)
      |
      v
[ Internal Core Services ]
  |-- ECS (Component Management + Locking)
  |-- SmartScheduler (Cyclic/Delayed Tasks)
  |-- SystemExecution (Automated ECS Loops)
  |-- WorkScheduler (Multithreaded Tasks)
  |-- EventBus (Global Events)
  |-- SQLDB (Persistence)
  |-- VFS (Virtual File System)
  |-- MemoryAlloc (Custom Memory Management)
  |-- ModuleLoader (Dynamic Module Loading)
  |-- ContainerLoader (Resource Container Handling)
  |-- Clock (Precision Timing)
```

### Key Principles

1.  **Packet-Driven**: No direct function calls to core services. All requests are sent as `FURCMDPacket`s.
2.  **Core Isolation**: Internal engine headers are never exposed to modules. Only POD contexts and C-style interfaces are shared.
3.  **Concurrency First**: The engine uses a worker-thread model for command execution and a dedicated scheduler for heavy compute tasks.
4.  **ABI Stability**: By using C-entry points and avoiding STL in interfaces, modules can be loaded dynamically without binary compatibility issues.

## 🛠 Build and Run

### Build (out-of-source):

```bash
mkdir -p build
cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Run:

```bash
./archetyped
```

## 🛠 Toolchain: forge

Archetyped includes `forge`, a powerful CLI toolchain designed to automate the development lifecycle, from module creation to container orchestration.

### 🚀 Key Capabilities

`forge` operates in two modes: **Command Line** (e.g., `./build/forge cont build <name>`) and **Interactive Shell** (run `./build/forge` without arguments — supports history, arrow keys, and a persistent container context via `use <name>`).

#### 🏗 Core & Toolchain Management
- `engine-build [--recache]` — Compiles the engine core.
- `forge-build` — Rebuilds the `forge` toolchain itself.
- `test` — Executes SDK integration tests to verify system stability.

#### 🧩 Modules (noun-group: `mod`)
- `mod scan` — Scans the modules directory and updates the internal project database.
- `mod list` — Lists all discovered modules with their versions.
- `mod info <name>` — Displays detailed metadata for a specific module.
- `mod new <name>` — Scaffolds a new module from a template.

#### 🗃 Containers (noun-group: `cont`)
Containers define a specific set of modules and settings to be loaded by the engine.
- `cont new <name>` — Initializes a new container configuration.
- `cont add <c> <m>` / `cont rm <c> <m>` — Manages the module load order within a container.
- `cont build <c> [--update-assets] [--src-included]` — Compiles all modules in the container and packs them into a deployable structure.
- `cont run <c>` — Sets the active container and launches the engine.
- `cont info <name>` — Shows the composition and metadata of a container.
- `cont cp`, `cont delete`, `cont archive`, `cont unarchive` — Container lifecycle utilities.

#### 🔗 SDK (noun-group: `sdk`)
- `sdk init <module> <unit> [ver]` — Creates an SDK provider skeleton.
- `sdk sync [--check] [--list]` — Vendors SDK headers with transitive dependency resolution.
- `sdk check` — Verifies SDK integrity across all consumers.
- `sdk deps [module]` — Shows the SDK dependency graph.

### 🔄 Typical Workflow

1.  **Build Core**: `forge engine-build`
2.  **Create Module**: `forge mod new MyAwesomeModule` → *Implement code*
3.  **Setup Container**: `forge cont new DevContainer` → `forge cont add DevContainer MyAwesomeModule`
4.  **Deploy & Run**: `forge cont build DevContainer` → `forge cont run DevContainer`

Legacy flat command names (`init-mod`, `add-mod`, `sync-sdk`, …) still work as aliases.

## 📖 Documentation

For detailed API usage and module development guides, see:
*   [API Documentation](include/Engine/API_DOCUMENTATION.md)
*   [Russian API Documentation](include/Engine/API_DOCUMENTATION_RU.md)
