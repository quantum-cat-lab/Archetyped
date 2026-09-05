# Archetyped: Creating Custom Services

Archetyped's architecture makes it easy to add your own services by registering FUR methods in the kernel.

## Step 1: Define the Service Logic

Create a class or a set of functions that will handle the commands.

```cpp
// MyService.h
void MyCustomMethod(FURCMDPacket& packet) {
    // 1. Get input from packet.payload
    // 2. Perform logic
    // 3. (Optional) Write result to packet.outputBuffer
    // 4. Set packet.fence to 1 if present
    if (packet.fence) {
        *packet.fence = 1;
    }
}
```

## Step 2: Register the Service in the Kernel

You need to register your methods with a unique 32-bit hash.

```cpp
uint32_t myMethodHash = fnv1aHashConst("my_engine:my_service:my_method");
kernel->registerCMDMethod(myMethodHash, MyCustomMethod);
```

## Step 3: Create an SDK Wrapper (Optional but Recommended)

For ease of use, create a C++ class that wraps the FUR packet creation.

```cpp
class MyService {
public:
    void doSomething(int value) {
        Ticket* ticket = SDK::Get()->allocateTicket();
        
        FURCMDPacket packet;
        packet.methodHash = myMethodHash;
        packet.payload = &value;
        packet.payloadSize = sizeof(int);
        packet.fence = (uint64_t*)&ticket->fence;

        SDK::Get()->sendPacket(packet);
        
        // Wait for completion
        while(!ticket->isReady()) {}
    }
};
```

## Best Practices

1.  **Context Structs**: Use structs for payloads if you need to pass multiple arguments.
2.  **Thread Safety**: Remember that FUR methods are executed in a worker thread. Ensure your service logic is thread-safe.
3.  **Naming**: Use a clear namespace for your method hashes to avoid collisions (e.g., `author:service:method`).
