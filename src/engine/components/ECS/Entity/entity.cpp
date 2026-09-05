#include "entity.h"

Entity EntityManager::createEntity() {
    uint32_t id;
    uint32_t version;
    
    if (!freeIds.empty()) {
        id = freeIds.back();
        freeIds.pop_back();
        version = versions[id];
    } else {
        id = nextEntityId++;
        versions.push_back(1);
        active.push_back(true);
        version = 1;
    }
    
    if (id >= active.size()) {
        active.resize(id + 1, false);
    }
    active[id] = true;
    activeCount++;
    return Entity{ id, version };
}

void EntityManager::destroyEntity(Entity e) {
    if (isValid(e)) {
        active[e.id] = false;
        versions[e.id]++;
        freeIds.push_back(e.id);
        activeCount--;
    }
}

std::optional<Entity> EntityManager::getEntityById(uint32_t id) {
    if (id < versions.size() && active[id]) {
        return Entity{ id, versions[id] };
    }
    return std::nullopt;
}

bool EntityManager::isValid(Entity e) const {
    return e.id < versions.size() && active[e.id] && versions[e.id] == e.version;
}

uint32_t EntityManager::getEntityCount() const {
    return activeCount;
}

