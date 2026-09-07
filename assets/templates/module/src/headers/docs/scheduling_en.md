# Archetyped: Scheduling and System Execution

These services provide mechanisms for running periodic logic, delayed tasks, and high-level ECS-like systems.

## Smart Scheduler

The Smart Scheduler handles timed tasks and "ticks".

### FUR Methods

-   `scheduleCyclicHash`: Runs a task every `N` milliseconds.
-   `scheduleDelayedHash`: Runs a task once after a delay.
-   `scheduleTickHash`: Runs a task every engine tick.

### Example (Using FUR directly)

```cpp
#include "Engine/FractalSDK.h"

void MyPeriodicTask(void* ctx) {
    // Logic...
}

void Setup() {
    scheduleSSCMDContext ctx;
    ctx.fn = MyPeriodicTask;
    ctx.context = nullptr;
    ctx.value = 1000; // 1000ms interval

    FURCMDPacket packet;
    packet.methodHash = scheduleCyclicHash;
    packet.payloadSize = sizeof(ctx);
    packet.payload = &ctx;

    FractalSDK::SDK::Get()->sendPacket(packet);
}
```

## System Execution

The System Execution service is a higher-level wrapper that uses the Smart Scheduler to run game logic systems.

### FUR Methods

-   `registerSystemHash`: Registers a function to be called at a specific frequency.

### Example

```cpp
void MySystem(void* ctx) {
    // Update logic...
}

void Register() {
    registerSystemCMDContext ctx;
    ctx.fn = MySystem;
    ctx.context = nullptr;
    ctx.frequencyMs = 0; // 0 for every tick

    FURCMDPacket packet;
    packet.methodHash = registerSystemHash;
    packet.payloadSize = sizeof(ctx);
    packet.payload = &ctx;

    FractalSDK::SDK::Get()->sendPacket(packet);
}
```
