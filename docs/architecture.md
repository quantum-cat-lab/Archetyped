# Architecture

Archetyped is a C++23 service-oriented engine core. The mental model is
"small kernel + many isolated domains" — every subsystem is a service
with a stable ABI, addressable through a 32-bit hash, dispatchable on a
worker pool.

## Command routing (FUR)

The **F**ast **U**tility **R**outer (`FUR`) is the only way modules talk
to engine services. It is a buffered, asynchronous dispatcher.

```text
   ┌───────────────────────────────────────────────┐
   │              Module (C++ / C# / Kotlin)       │
   │                                               │
   │   FURCMDPacket pkt{                           │
   │       methodHash:  registerCMDomainHash,      │
   │       payload:     &domainId,                 │
   │       payloadSize: sizeof(domainId),          │
   │       outputBuffer: &result,                  │
   │       fence:       &ticket->fence,            │
   │   };                                          │
   │   IKernel::sendPacket(pkt);                   │
   └────────────────────────┬──────────────────────┘
                            ▼
   ┌───────────────────────────────────────────────┐
   │   Kernel / FUR worker thread pool             │
   │   ─ routes methodHash → handler via hash map  │
   │   ─ copies payload into pinned arena          │
   │   ─ calls handler(payload, outputBuffer)      │
   │   ─ sets *fence = 1                           │
   └────────────────────────┬──────────────────────┘
                            ▼
   ┌───────────────────────────────────────────────┐
   │   Engine service                              │
   │   (ECS / Event / VFS / WorkScheduler / …)     │
   └───────────────────────────────────────────────┘
```

Properties:

- **No virtuals on the hot path.** A `dispatch` table maps `uint32_t → fn*`,
  fetched with one cache-line load.
- **Bounded latency.** Single-producer/single-consumer per packet; the
  module's thread may continue immediately if it doesn't need a result.
- **POD payloads.** Every packet carries an opaque pointer and a size;
  the kernel never dereferences a module pointer directly.
- **Output buffer protocol.** A handler writes its result into the
  caller's `outputBuffer`; the caller never re-enters the engine to
  fetch it.
- **Tickets & fences.** For synchronous calls, the kernel hands out a
  `Ticket` (one cache line, atomically flipped to `isReady=true`).
  The caller spins once before reading — no kernel roundtrip.

## Service tiers

The 24 components under `src/engine/components/` split into four tiers
by lifetime and multiplicity. The naming convention makes the tier
visible in the class name.

### Tier 1 — Singletons (suffix `Host` / `Scheduler` / domain-less)

One instance per engine process. Responsible for **process-wide** state
or for loading a backing runtime.

| Class | Role |
|---|---|
| `ClrHost` | load CoreCLR, hand out ALCs |
| `JvmHost` | load libjvm.so, hand out JVMs |
| `BridgeRegistry` | kernel-global cache of C++↔managed function pointers |
| `SmartScheduler` | cyclic + delayed task scheduler |
| `SystemExecution` | registerSystem wrapper for `SystemExecution` |
| `Clock` | process time |
| `ContainerLoader` | tracks the active container |
| `VFS` (static) | global mount table |
| `VFSDomains` | in-process VFS mount bookkeeping |

Tier 1 also includes the per-runtime **worker pools** (one pool per
hosted language):
`ClrScheduler`, `JvmScheduler` — both backed by `workScheduler/TaskQueue.h`.

### Tier 2 — Factories (suffix `Factory`)

One instance per kernel that **owns a map of per-domain instances**.
A "domain" is a per-module or per-feature namespace; the factory owns
its lifecycle.

| Class | Owns per-domain |
|---|---|
| `ClrFactory` | `ClrInstance` (ALC + assembly cache) |
| `JvmFactory` | `JvmInstance` (JVMEnv + classloader) |
| `CSharpFactory` | `CSharpInstance` (DLL + entry type) |
| `KotlinFactory` | `KotlinInstance` (JAR + main class) |
| `SchemaFactory` | `SchemaInstance` (per-domain struct registry) |
| `CMFactory` | `ComponentInstance` + `VaultInstance` (per-domain ECS) |
| `EBFactory` | `EventInstance` (per-domain event bus) |
| `WSFactory` | per-domain `WorkScheduler` |
| `SQLFactory` | `SqlInstance` (per-domain sqlite3 handle) |
| `ModuleFactory` | `ModuleInstance` (per-loaded .so) |

A factory exposes `createInstance(name, …)`, `getInstance(name)`,
`hasInstance(name)`, `destroyInstance(name)`, `enumInstances()`,
`instanceCount()`. The Tier-1 host is created once; the factory
hands out many instances.

### Tier 3 — Instances (suffix `Instance`)

The POJO state a factory hands out. Lives entirely behind a
`createInstance` / `destroyInstance` boundary; the factory owns the
allocation, the kernel owns the factory, the module owns the **call**
through the SDK.

```text
Module ─[SDK call]─► ClrFactory ─[create]─► ClrInstance
                                ◄──[get/destroy]──┘
```

### Tier 4 — Per-kernel stores (suffix `Store` / `Registry`)

Kernel-private caches. Not reachable from the SDK; not domain-scoped.

| Class | Role |
|---|---|
| `MutexStore` | per-kernel mutex slot pool |
| `SubmoduleRegistry` | tracks submodules attached to a host |

### Cross-cutting

- `BridgeRegistry` lives in Tier 1 but is **per-kernel** in spirit —
  it's a cache, not a process-global singleton in the strict sense.
- `VFS` itself is static (a global mount table); `VFSDomains` is the
  per-domain bookkeeping that the static class delegates to.

## Hash protocol

Every public method has a 32-bit FNV-1a hash of the form
`"archetyped:<group>:<verb>"`. The full table lives in
`include/SDK/archetyped/FractalSDK.h`. Examples:

```
archetyped:ecs:registerCMDomain       → 0x…
archetyped:ecs:registerComponent      → 0x…
archetyped:event_bus:subscribeEvent   → 0x…
archetyped:work_scheduler:scheduleTask→ 0x…
archetyped:module_loader:loadModule   → 0x…
archetyped:clr:registerSchema         → 0x…
```

The hash space is shared across engine services and C# runtime. Two
methods with the same hash are treated as the same method — namespacing
in the string is convention only.

## Runtimes

The engine core is pure C++23. Modules can additionally ship code in:

- **C# / .NET** — `ClrHost` calls `hostfxr` to load CoreCLR, then asks
  `Bootstrap.cs` (vendored at `src/main/csharp/Bootstrap/`) for a
  collectible `AssemblyLoadContext`. Each module gets its own ALC; ALCs
  see other ALCs through an `imports` table declared in
  `configs/active_container.json`.
- **Kotlin / JVM** — `JvmHost` calls `JNI_CreateJavaVM`; `KotlinFactory`
  hands out per-domain classloaders. JARs are loaded as resources
  through the VFS.

**Lua is intentionally not supported.** C# and Kotlin cover the
hot-reload scripting use case; bringing Lua back in would require
thread-state ownership rules that the rest of the engine doesn't have.

## Containers

A **container** is a runnable image: a list of modules + their order +
their `assets/` and `configs/`. The build is incremental and hash-cached
(`forge cont build` skips modules whose sources haven't changed).

The engine binary itself is loaded once; modules are `dlopen`'d in the
order specified by the container's stages. See
[`forge.md`](forge.md#container-workflow-forge-cont-) for the stage
ordering.

## Memory

The engine uses arena allocation per request. A `Ticket` is the only
out-of-arena allocation for packet results; the arena is reset after the
handler returns. This means a module that drops a `FURCMDPacket*` into
the void loses nothing — there's no per-call malloc.

Custom allocators (`memory/EngineArena.h`, `memory/DynamicArray.hpp`)
are exposed through the SDK so modules can opt in.

## Why this design?

- **ABI stability.** A module built today loads in an engine built
  three years from now, as long as the SDK hash table doesn't change.
- **No shared globals.** Each service is an instance, owned by a
  factory, scoped to a domain. Two modules asking for an ECS get two
  ECSes, not one global.
- **No virtuals on the hot path.** Dispatch is `hash → fn*`; even
  cross-language calls are just function pointers.
- **One transport.** Whether the call comes from C++, C#, or Kotlin,
  it goes through `FURCMDPacket`. There is no "C++ binding" vs
  "C# binding" — both compile to the same `fnv1aHashConst` + payload.
- **Bounded concurrency.** All work happens on the engine's worker
  pools; module code that wants to do heavy lifting just submits a
  task to `WSFactory`.
