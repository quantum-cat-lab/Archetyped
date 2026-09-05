#pragma once
#include <SDK/archetyped/hash/hash.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "ankerl/unordered_dense.h"
#include "../MemoryAlloc/EngineArena.h"

/**
 * EventData - Data associated with an event.
 */
struct EventData {
    void* ptr;
    size_t size;
};

/**
 * EventCallback - Callback function type for event subscribers.
 * 
 * @param eventId The ID of the event.
 * @param data The data associated with the event.
 * @param user User-provided pointer.
 */
typedef void (*EventCallback)(uint32_t, const EventData&, void*);

/**
 * Subscriber - Represents a subscriber to a specific event.
 */
struct Subscriber {
    EventCallback cb;
    void* user;
    uint32_t subscriberId;
};

/**
 * QueuedEvent - Represents an event that is queued for processing.
 */
struct QueuedEvent {
    uint32_t id;
    EventData data;
};

/**
 * EventInstance (Event Bus) - Core Component.
 * Responsible for managing event registration, subscription, and distribution.
 */
class EventInstance {
public:
    ~EventInstance() = default;
    /**
     * Initializes the EventInstance.
     */
    EventInstance();

    /**
     * Registers a new event ID.
     * 
     * @param eventId The ID of the event to register.
     */
    void registerEvent(uint32_t eventId);

    /**
     * Reserves memory for subscribers.
     * 
     * @param size The number of subscribers to reserve.
     */
    void reserveSubscribers(size_t);

    /**
     * Subscribes to a specific event.
     * 
     * @param eventId The ID of the event to subscribe to.
     * @param cb The callback function to be called when the event is emitted.
     * @param user User-provided pointer passed to the callback.
     * @param subscriberId Optional subscriber ID for unsubscription.
     */
    void subscribe(uint32_t eventId, EventCallback cb, void* user = nullptr, uint32_t subscriberId = 0);

    /**
     * Emits an event immediately to all subscribers.
     * 
     * @param eventId The ID of the event to emit.
     * @param data The data associated with the event.
     */
    void emitEvent(uint32_t eventId, const EventData& data);

    /**
     * Pushes an event to the queue for later processing.
     * 
     * @param eventId The ID of the event to push.
     * @param data The data associated with the event.
     */
    void pushEvent(uint32_t eventId, const EventData& data);

    /**
     * Processes all queued events.
     */
    void processEvents();

    /**
     * Resets the event bus, clearing subscribers and queues.
     */
    void reset();

    /**
     * Unsubscribes a subscriber using its ID.
     * 
     * @param subscriberId The ID of the subscriber to remove.
     */
    void unsubscribe(uint32_t subscriberId);

private:
    ankerl::unordered_dense::map<uint32_t, std::vector<Subscriber>, IdentityHash> subscribers;
    std::unique_ptr<EngineArena> eventArena;
    std::vector<QueuedEvent> frontBuffer;
    std::vector<QueuedEvent> backBuffer;
};
