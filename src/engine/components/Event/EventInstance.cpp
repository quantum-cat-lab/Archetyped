#include "EventInstance.h"
#include <cstdint>

EventInstance::EventInstance() : eventArena(std::make_unique<EngineArena>(1024 * 1024))
{
    backBuffer.reserve(1024);
    frontBuffer.reserve(1024);
}
void EventInstance::registerEvent(uint32_t eventId) {
    subscribers[eventId] = std::vector<Subscriber>();
}
void EventInstance::subscribe(uint32_t eventId, EventCallback cb, void* user, uint32_t subscriberId) {
    subscribers[eventId].push_back({ cb, user, subscriberId });
}
void EventInstance::pushEvent(uint32_t eventId, const EventData& data) {
    backBuffer.push_back({ eventId, data });
}
void EventInstance::emitEvent(uint32_t eventId, const EventData& data) {
    auto it = subscribers.find(eventId);
    if (it != subscribers.end()) {
        for (const auto& sub : it->second) {
            sub.cb(eventId, data, sub.user);
        }
    }
}
void EventInstance::processEvents() {
    if (backBuffer.empty()) return;
    frontBuffer.swap(backBuffer);
    while (!backBuffer.empty()){
        frontBuffer.clear();
        frontBuffer.swap(backBuffer);

        for(const auto& evt : frontBuffer) {
            auto it = subscribers.find(evt.id);
            if (it != subscribers.end()) {
                for (const auto& sub : it->second) {
                    sub.cb(evt.id, evt.data, sub.user);
                }
            }
        }
    }
}
void EventInstance::reset() {
    frontBuffer.clear();
    backBuffer.clear();
    eventArena->reset();
}
void EventInstance::unsubscribe(uint32_t subscriberId) {
    for (auto& [eventId, subs] : subscribers) {
        subs.erase(std::remove_if(subs.begin(), subs.end(), [subscriberId](const Subscriber& sub) {
            return sub.subscriberId == subscriberId;
        }), subs.end());
    }
}