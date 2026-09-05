#pragma once
#ifndef ENTITY_H
#define ENTITY_H
#include <cstdint>
#include <unordered_set>
#include <vector>
#include <optional>

/**
 * Entity (Entity) - Core Component.
 * Represents a unique entity in the ECS.
 */
struct Entity
{
    uint32_t id;
    uint32_t version;
    
    bool operator==(const Entity& other) const {
        return id == other.id && version == other.version;
    }
};

namespace std {
    template<>
    struct hash<Entity> {
        size_t operator()(const Entity& e) const {
            return hash<uint32_t>()(e.id) ^ (hash<uint32_t>()(e.version) << 1);
        }
    };
}

/**
 * EntityManager (Entity Manager) - Core Component.
 * Manages the creation and destruction of entities.
 */
class EntityManager {
public:
    /**
     * Creates a new unique entity.
     * 
     * @return The newly created Entity.
     */
    Entity createEntity();
    /**
     * Destroys the specified entity and marks its ID for reuse.
     * 
     * @param e The entity to destroy.
     */
    void destroyEntity(Entity e);
    
    /**
     * Attempts to retrieve an entity by its unique ID.
     * 
     * @param id The ID of the entity to retrieve.
     * @return An optional containing the Entity if found, otherwise std::nullopt.
     */
    std::optional<Entity> getEntityById(uint32_t id);
    /**
     * Checks if a given entity is currently active and valid.
     * 
     * @param e The entity to validate.
     * @return true if the entity is valid, false otherwise.
     */
    bool isValid(Entity e) const;
    /**
     * Gets the total number of currently active entities.
     * 
     * @return The count of active entities.
     */
    uint32_t getEntityCount() const;
    
private:
    uint32_t nextEntityId = 0;
    uint32_t activeCount = 0;
    std::vector<uint32_t> versions;
    std::vector<bool> active;
    std::vector<uint32_t> freeIds;
};
#endif
