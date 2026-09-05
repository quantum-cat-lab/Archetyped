# Archetyped: Entity-Component-System (ECS)

The ECS service provides a way to manage entities and their components in a high-performance, data-oriented way.

## Overview

-   **Entity**: A simple ID representing an object.
-   **Component**: Pure data associated with an entity.
-   **System**: Logic that processes entities with specific components.

The ECS in Archetyped is domain-based. Each domain acts as an independent "world".

## ECS Class Methods

### Initialization

#### `ECS(std::string name)`
Registers a new ECS domain with the given name.
-   **Example**: `ECS myWorld("GameWorld");`

### Component Registration

#### `registerComponent<T>(uint32_t hashId, uint32_t capacity)`
Registers a component type `T` with a specific hash ID and initial capacity.
-   **Example**: `myWorld.registerComponent<Position>(fnv1aHashConst("Position"), 1000);`

### Entity Management (Deferred)

These commands are queued and executed when `flushCommands()` is called.

#### `attachComponentDeferred<T>(Entity entity, uint32_t componentHashId, T* componentData)`
Queues an attachment of component `T` to the entity.
-   **Example**: `myWorld.attachComponentDeferred(player, posHash, &initialPos);`

#### `removeComponentDeferred<T>(Entity entity, uint32_t componentHashId)`
Queues removal of a component from the entity.
-   **Example**: `myWorld.removeComponentDeferred<Position>(player, posHash);`

#### `flushCommands()`
Executes all queued attachment/removal commands.
-   **Note**: This is a blocking operation (waits for completion).

### Querying Data

#### `getComponent<T>(Entity entity, uint32_t componentHashId)`
Returns a pointer to the component data for the given entity.
-   **Example**: `Position* p = myWorld.getComponent<Position>(player, posHash);`

#### `hasComponent(Entity entity, uint32_t componentHashId)`
Checks if the entity has the specified component.
-   **Example**: `bool alive = myWorld.hasComponent(player, healthHash);`

#### `getRawPtr(uint32_t componentId, bool lock = false)`
Returns a raw pointer to the entire component array for high-speed iteration.
-   **Note**: Setting `lock = true` will acquire a mutex for this component array.

#### `getGroupSize(std::initializer_list<uint32_t> hashes)`
Returns the number of entities that have ALL the specified components.

#### `contains(std::initializer_list<uint32_t> array, uint32_t value)`
Utility to check if a value exists in a hash list.

### Asynchronous Operations

These methods return a `Ticket*` and do not block the calling thread.

-   `getComponentAsync(Entity entity, uint32_t componentHashId, void* outputBuffer)`
-   `hasComponentAsync(Entity entity, uint32_t componentHashId, void* outputBuffer)`
-   `getGroupSizeAsync(std::vector<uint32_t> hashes, void* outputBuffer)`
-   `getRawPtrAsync(uint32_t componentId, bool lock, void* outputBuffer)`

### Thread Safety

#### `setLock(uint32_t componentId, bool lock)`
Manually locks/unlocks a component array for safe access from other threads.

#### `isLocked(uint32_t componentId)`
Checks if a component array is currently locked.

## Usage Example

```cpp
#include "Engine/ECS.h"

struct Position { float x, y; };
constexpr uint32_t posHash = fnv1aHashConst("Position");

void GameLoop() {
    ECS world("MainWorld");
    world.registerComponent<Position>(posHash, 1000);

    Entity player = { 1 };
    Position startPos = { 0, 0 };

    // Attach component
    world.attachComponentDeferred(player, posHash, &startPos);
    world.flushCommands();

    // Query component
    Position* p = world.getComponent<Position>(player, posHash);
    if (p) {
        p->x += 1.0f;
    }
}
```
