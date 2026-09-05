# SDK

The **SDK** is the C/C++ header set under `include/SDK/archetyped/`. It is
the **only** contract between the engine and its modules. Everything in
there is stable; everything outside (`src/engine/`) is implementation
detail.

## Layout

```
include/SDK/archetyped/
├── FractalSDK.h          packet types + method hashes
├── IKernel.h             entry-point surface (sendPacket, registerCMDMethod)
├── ECS.h                 ECS typed wrapper (CSFactory, VaultInstance)
├── EventInstance.h       Event bus (EBFactory)
├── SQLDB.h               SQLite-per-domain (SQLFactory)
├── VFS.h + LocalVFS.hpp  virtual file system (VFS / VFSDomains)
├── WorkScheduler.h       multithreaded tasks (WSFactory)
├── SmartScheduler.h      cyclic / delayed tasks
├── SystemExecution.h     registerSystem wrapper
├── ModuleLoader.h        load/unload/query modules
├── Clock.h               time queries
├── ComponentRegistry.h   component metadata table
├── VaultInstance.h       per-domain vault (factory-bound)
├── SchemaBlock.h         structured payload layout
├── schema/SchemaRegistryPayload.h
├── CLR.h, JVM.h          managed-runtime entry points
├── csharp.h, csharp.hpp  .NET-side SDK
├── jvm.h                 JVM-side SDK
├── memory/               EngineArena, alloc_utils, DynamicArray, …
├── hash/hash.h           FNV-1a + hash macros
└── env/                  cross-platform headers (dllimport/export)
```

## Distribution

`forge sdk sync` reads `configs/sdk-manifest.json` and copies the listed
files into every module's `include/SDK/archetyped/`. The vendored copy is
**identical** to the engine source — no patching, no rewriting. This
guarantees modules compile against exactly the headers the engine was
built with.

```bash
forge sdk sync                # write to all modules
forge sdk sync --force        # overwrite files that exist (default: skip)
forge sdk sync --module foo   # only module 'foo'
forge sdk check               # report drift
```

After a `forge engine-build` the template module
(`assets/templates/module/include/SDK/archetyped/`) is also refreshed, so
`forge mod new` always gets the current headers.

## The hash protocol

Every engine service is reached through a method **hash**: a 32-bit
FNV-1a of a string like `"archetyped:ecs:registerComponent"`. The hash is
the only thing that crosses the FFI boundary — the SDK header
`FractalSDK.h` exposes a `constexpr` for every method:

```cpp
#include <SDK/archetyped/FractalSDK.h>
#include <SDK/archetyped/hash/hash.h>

constexpr uint32_t hash = registerCMDomainHash;   // archetype:ecs:registerCMDomain

FURCMDPacket pkt;
pkt.methodHash  = hash;
pkt.payload     = &myPayload;
pkt.payloadSize = sizeof(myPayload);
pkt.fence       = nullptr;  // fire-and-forget, or &ticket->fence for sync
```

You don't compute hashes yourself in production code — always use the
`constexpr` from `FractalSDK.h`. The only place raw `fnv1aHashConst("...")`
appears is engine code defining the methods.

## Payloads

Method payloads are POD structs defined alongside the `constexpr` in
`FractalSDK.h`. Examples:

```cpp
struct registerCMDomainPayload { uint32_t domainId; };
struct registerComponentPayload { uint32_t domainId, componentId, componentSize, capacity; };
struct subscribeEventPayload   { uint32_t domainId, eventId; EventCallback cb; void* user; uint32_t subscriberId; };
```

Layouts are stable. Adding a field is allowed (callers initialize to 0),
removing a field breaks ABI. The schema registry (`schema/SchemaBlock.h`)
tracks payload versions for runtime checks.

## Reading results

Two ways:

1. **Synchronous (fence)**: the SDK wrapper allocates a `Ticket`, sets
   `pkt.fence = &ticket->fence`, sends the packet, and busy-waits on
   `ticket->isReady()`. Result is read from the engine into a buffer you
   supply through `pkt.outputBuffer`.

   ```cpp
   bool has = ECS::hasComponent(entity, compId);   // SDK hides the dance
   ```

2. **Asynchronous (callback)**: pass a function pointer in the payload.
   The engine will invoke it on a worker thread when the result is ready.
   Used by `getComponentAsync`, `hasComponentAsync`, etc.

## ABI rules

- The SDK is **C-linkage by design**. Header-only inline functions
  use `extern "C"` for the public surface.
- `extern "C"` symbols in headers must be POD-only.
- `FractalSDK.h` contains zero STL types in the public structs.
- `Ticket` is opaque (forward-declared in the header, full type in
  `memory/EngineArena.h` which is included transitively).
- No exceptions across the boundary. SDK wrappers translate to return
  codes / output buffers.
- No thread-locals. Per-thread state lives in the engine's worker pools.

## Adding a new SDK method

1. Add a payload struct in `include/SDK/archetyped/FractalSDK.h`.
2. Add a `constexpr uint32_t myMethodHash = fnv1aHashConst("archetyped:foo:bar");`.
3. Add the method to `src/engine/...` (the handler).
4. Add an inline wrapper in the relevant typed header (`ECS.h`,
   `EventInstance.h`, …) that builds the packet, allocates a ticket,
   and returns the result.
5. Add the header path to `configs/sdk-manifest.json`.
6. `forge sdk sync && forge sdk check`.

The new method is now callable from every module.

## Versioning

`configs/sdk-manifest.json` records the SDK version per provider:

```json
{
  "provides": [
    { "name": "archetyped", "version": "1.0.0", "headers": [...] }
  ]
}
```

Bump `version` when an existing payload's layout changes. The schema
registry's hash on each payload struct lets the engine detect
mismatches at first call and return a clear error.
