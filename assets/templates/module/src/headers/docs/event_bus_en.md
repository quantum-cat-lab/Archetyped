# Archetyped: Event Bus

The Event Bus service provides a decoupled way for different parts of the application to communicate via events.

## Overview

The Event Bus uses a publisher-subscriber pattern. You can emit events immediately or push them to a queue for later processing.

## EventBus Class Methods

### Initialization

#### `EventBus(std::string name)`
Registers a new Event Bus domain.
-   **Example**: `EventBus engineEvents("EngineEvents");`

### Subscription

#### `subscribe(uint32_t eventId, EventCallback callback, void* user = nullptr, uint32_t subscriberId = 0)`
Subscribes to an event.
-   `eventId`: Hash of the event name.
-   `callback`: Function pointer of type `void (*)(uint32_t eventId, const EventData& data, void* user)`.
-   `user`: Optional user data passed to the callback.
-   `subscriberId`: Optional unique ID for the subscriber (used for unsubscription).

### Emitting Events

#### `emit(uint32_t eventId, const EventData& data)`
Immediately invokes all subscribers for the given event. This is a blocking operation.

#### `push(uint32_t eventId, const EventData& data)`
Pushes the event to a queue. It will be processed when `process()` is called. This is non-blocking.

#### `process()`
Processes all events in the queue.

### Management

#### `unsubscribe(uint32_t subscriberId)`
Removes a subscriber by its ID.

#### `reset()`
Clears all subscribers and events in the domain.

## Usage Example

```cpp
#include "Engine/EventBus.h"

void OnPlayerSpawned(uint32_t id, const EventData& data, void* user) {
    printf("Player spawned!\n");
}

void Example() {
    EventBus bus("GameBus");
    uint32_t spawnEvent = fnv1aHashConst("PlayerSpawned");

    // Subscribe
    bus.subscribe(spawnEvent, OnPlayerSpawned);

    // Emit immediately
    EventData d = { nullptr, 0 };
    bus.emit(spawnEvent, d);

    // Push to queue
    bus.push(spawnEvent, d);
    bus.process(); // Now the callback is called for the pushed event
}
```
