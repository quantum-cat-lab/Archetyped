# Archetyped: Introduction and Kernel

Archetyped is a high-performance, modular engine built on a minimalist kernel and a "Fast Utility Router" (FUR) command architecture.

## Architecture Overview

The engine consists of two main parts:
1.  **FractalKernel**: A central hub that routes commands between different modules and services.
2.  **Services**: Independent modules (ECS, SQLDB, EventBus, etc.) that register their methods in the kernel.

Communication between the SDK (or other modules) and the services is done through **FUR Commands**.

## The FUR Command System

Every operation in the engine is encapsulated in a `FURCMDPacket`.

### FURCMDPacket Structure

```cpp
struct FURCMDPacket {
    uint32_t methodHash;    // FNV1a hash of the method name (e.g., "archetyped:ecs:registerComponent")
    uint16_t payloadSize;   // Size of the data being sent
    uint16_t flags;         // System flags
    void* payload;          // Pointer to the input data (context struct)
    void* outputBuffer;     // Pointer to a buffer for return values
    uint64_t* fence;        // Pointer to an atomic synchronization flag (Ticket)
};
```

### Synchronization (Tickets)

Since the kernel processes commands asynchronously, the engine uses **Tickets** to track task completion.

-   **Ticket**: Contains an atomic `fence`. When the engine finishes a task, it sets the fence to 1.
-   **TicketPool**: A pre-allocated pool of tickets to avoid runtime allocations.

**Example: Waiting for a command to finish**
```cpp
Ticket* ticket = FractalSDK::SDK::Get()->allocateTicket();
packet.fence = (uint64_t*)&ticket->fence;

SDK::Get()->sendPacket(packet);

while(!ticket->isReady()) {
    // Wait or do other work
}
```

## Initialization

Before using any engine services, you must initialize the `FractalSDK`.

```cpp
#include "Engine/FractalSDK.h"

// Assuming 'kernel' is provided by the engine host
FractalSDK::SDK::Initialize(kernel);

// ... use engine services ...

FractalSDK::SDK::Shutdown();
```

## Core Principles

-   **Type Safety**: While FUR packets use `void*`, the high-level wrappers (ECS, SQLDB) provide type-safe templates.
-   **Asynchronicity**: Most commands can be sent without blocking the main thread.
-   **Decoupling**: Services don't know about each other; they only communicate through the Kernel.
