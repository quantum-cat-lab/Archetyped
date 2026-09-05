# Archetyped: Work Scheduler

The Work Scheduler service manages a thread pool for executing background tasks.

## Overview

You can offload heavy computations to the Work Scheduler to keep the main thread responsive.

## WorkScheduler Class Methods

### Initialization

#### `WorkScheduler(std::string name)`
Registers a new Work Scheduler domain.

### Task Scheduling

#### `schedule(w_task_fn fn, void* context)`
Schedules a task for execution in the thread pool.
-   `fn`: Function pointer of type `void (*)(void* context)`.
-   `context`: Pointer to data needed by the function.

## Usage Example

```cpp
#include "Engine/WorkScheduler.h"

struct MyTaskData {
    int result;
};

void ComplexCalculation(void* ctx) {
    MyTaskData* data = static_cast<MyTaskData*>(ctx);
    // Do heavy work...
    data->result = 42;
}

void Example() {
    WorkScheduler scheduler("HeavyTasks");
    MyTaskData data;

    scheduler.schedule(ComplexCalculation, &data);

    // The task runs in the background.
    // Use Tickets or other sync mechanisms if you need to wait for it.
}
```
