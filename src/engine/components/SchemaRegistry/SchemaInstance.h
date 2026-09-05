#pragma once

#include <SDK/archetyped/schema/SchemaBlock.h>
#include <SDK/archetyped/hash/hash.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include "ankerl/unordered_dense.h"

struct FieldDef {
    uint32_t typeId;      // SCHEMA_* or FNV-1a of nested schema
    uint32_t offset;      // byte offset
    uint32_t size;        // byte size
    std::string name;     // field name
    uint64_t nameHash;    // FNV-1a of field name
};

struct SchemaDef {
    uint32_t hash;        // FNV-1a of schema name
    std::string name;     // display name
    std::vector<FieldDef> fields;
    uint32_t size = 0;    // schema size
    uint32_t align = 1;   // alignment
};

class SchemaInstance {
public:
    SchemaInstance() = default;
    ~SchemaInstance() = default;
    static SchemaInstance& instance();

    void registerSchema(std::string_view name, const std::vector<std::pair<uint32_t, std::string>>& fields);

    const SchemaDef* getSchema(uint32_t hash) const;
    const SchemaDef* getSchema(std::string_view name) const;

    uint32_t getSchemaFieldOffset(uint32_t schemaHash, uint64_t fieldHash) const;

    std::vector<uint8_t> dumpSchema() const;

    // Merge schemas from another registry (for compose/import). Skips existing hashes. TODO: add overriding (optional overriding :3 )
    void mergeFrom(const SchemaInstance& other);
    bool hasSchema(uint32_t hash) const;
    std::vector<uint32_t> schemaHashes() const;

private:
    SchemaInstance(const SchemaInstance&) = delete;
    SchemaInstance& operator=(const SchemaInstance&) = delete;

    uint32_t alignOf(uint32_t typeId) const;
    uint32_t sizeOf(uint32_t typeId) const;

    ankerl::unordered_dense::map<uint32_t, SchemaDef> schemas_;
    mutable std::mutex m_;
    friend class SchemaFactory;
};