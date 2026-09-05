#pragma once

#include <SDK/archetyped/schema/SchemaBlock.h>
#include <SDK/archetyped/hash/hash.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include "ankerl/unordered_dense.h"
#include "SchemaInstance.h"
#include <memory>

class SchemaFactory {
public:
    static SchemaFactory& instance();

    // Register schema in domain
    void registerSchema(std::string_view domain, std::string_view name, const std::vector<std::pair<uint32_t, std::string>>& fields);

    // Lookup by domain + hash
    const SchemaDef* getSchema(std::string_view domain, uint32_t hash) const;
    const SchemaDef* getSchema(std::string_view domain, std::string_view name) const;

    // Field offset in domain schema
    uint32_t getSchemaFieldOffset(std::string_view domain, uint32_t schemaHash, uint64_t fieldHash) const;

    // Dump all domains / single domain
    std::vector<uint8_t> dumpSchema(std::string_view domain) const;
    std::vector<std::vector<uint8_t>> dumpAllSchemas() const;

    // Import types from another domain (compose)
    void compose(std::string_view targetDomain, std::string_view importDomain);
    bool hasDomain(std::string_view domain) const;

    // Invalidate caches (hot-reload)
    void invalidateDomain(std::string_view domain);
    void invalidateAll();

private:
    SchemaFactory() = default;
    SchemaFactory(const SchemaFactory&) = delete;
    SchemaFactory& operator=(const SchemaFactory&) = delete;

    // Per-domain registries
    ankerl::unordered_dense::map<uint32_t, std::unique_ptr<SchemaInstance>> registries_;
    mutable std::mutex m_;
};