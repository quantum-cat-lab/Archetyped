#include "SchemaInstance.h"
#include <cstdint>
#include <cstring>
#include <SDK/archetyped/hash/hash.h>
#include <cstdio>

SchemaInstance& SchemaInstance::instance() {
    static SchemaInstance inst;
    return inst;
}

void SchemaInstance::registerSchema(std::string_view name, const std::vector<std::pair<uint32_t, std::string>>& fields) {
    std::lock_guard<std::mutex> lk(m_);

    SchemaDef def;
    def.hash = fnv1aHash(name);
    def.name = std::string(name);

    uint32_t offset = 0;
    for (const auto& [typeId, fieldName] : fields) {
        uint32_t sz = sizeOf(typeId);
        uint32_t al = alignOf(typeId);
        offset = (offset + al - 1) & ~(al - 1);

        def.fields.push_back(FieldDef{
            .typeId = typeId,
            .offset = offset,
            .size = sz,
            .name = fieldName,
            .nameHash = fnv1aHash(fieldName)
        });

        offset += sz;
    }

    uint32_t maxAlign = 1;
    for (const auto& f : def.fields) maxAlign = std::max(maxAlign, alignOf(f.typeId));
    def.size = (offset + maxAlign - 1) & ~(maxAlign - 1);
    def.align = maxAlign;

    schemas_[def.hash] = std::move(def);
}

const SchemaDef* SchemaInstance::getSchema(uint32_t hash) const {
    std::lock_guard<std::mutex> lk(m_);
    auto it = schemas_.find(hash);
    return it != schemas_.end() ? &it->second : nullptr;
}

const SchemaDef* SchemaInstance::getSchema(std::string_view name) const {
    return getSchema(fnv1aHash(name));
}

uint32_t SchemaInstance::getSchemaFieldOffset(uint32_t schemaHash, uint64_t fieldHash) const {
    std::lock_guard<std::mutex> lk(m_);
    auto it = schemas_.find(schemaHash);
    if (it == schemas_.end()) return 0;

    for (const auto& field : it->second.fields) {
        if (field.nameHash == fieldHash) {
            return field.offset;
        }
    }
    return 0;
}

uint32_t SchemaInstance::alignOf(uint32_t typeId) const {
    switch (typeId) {
        case SCHEMA_VOID: return 1;
        case SCHEMA_BOOL: return 1;
        case SCHEMA_U8: case SCHEMA_I8: return 1;
        case SCHEMA_U16: case SCHEMA_I16: return 2;
        case SCHEMA_U32: case SCHEMA_I32: case SCHEMA_F32: return 4;
        case SCHEMA_U64: case SCHEMA_I64: case SCHEMA_F64: return 8;
        case SCHEMA_PTR: case SCHEMA_HANDLE: case SCHEMA_STRUCT: return alignof(void*);
        default: return 1;
    }
}

uint32_t SchemaInstance::sizeOf(uint32_t typeId) const {
    switch (typeId) {
        case SCHEMA_VOID: return 0;
        case SCHEMA_BOOL: return 1;
        case SCHEMA_U8: case SCHEMA_I8: return 1;
        case SCHEMA_U16: case SCHEMA_I16: return 2;
        case SCHEMA_U32: case SCHEMA_I32: return 4;
        case SCHEMA_U64: case SCHEMA_I64: return 8;
        case SCHEMA_F32: return 4;
        case SCHEMA_F64: return 8;
        case SCHEMA_PTR: case SCHEMA_HANDLE: case SCHEMA_STRUCT: return sizeof(void*);
        default: return 0;
    }
}

std::vector<uint8_t> SchemaInstance::dumpSchema() const {
    std::lock_guard<std::mutex> lk(m_);
    if (schemas_.empty()) return {};
    std::vector<uint8_t> out;
    for (const auto& kv : schemas_) {
        const auto& def = kv.second;
        uint32_t strpoolSize = 0;
        for (const auto& field : def.fields) strpoolSize += static_cast<uint32_t>(field.name.size() + 1);
        size_t totalBytes = sizeof(SchemaBlock) + def.fields.size() * sizeof(FieldSchema) + strpoolSize;
        size_t base = out.size();
        out.resize(base + totalBytes);
        auto* sb = reinterpret_cast<SchemaBlock*>(out.data() + base);
        sb->hash = def.hash;
        sb->field_count = static_cast<uint32_t>(def.fields.size());
        sb->total_size = def.size;
        sb->strpool_size = strpoolSize;
        FieldSchema* fields = schemablock_fields(sb);
        char* strpool = schemablock_strpool(sb);
        uint32_t currentStrOffset = 0;
        for (size_t i = 0; i < def.fields.size(); ++i) {
            const auto& src = def.fields[i];
            auto& dst = fields[i];
            dst.type_id = src.typeId;
            dst.offset = src.offset;
            dst.size = src.size;
            dst.name_off = currentStrOffset;
            dst.name_len = static_cast<uint32_t>(src.name.size());
            std::memcpy(strpool + currentStrOffset, src.name.c_str(), dst.name_len + 1);
            currentStrOffset += dst.name_len + 1;
        }
    }
    return out;
}

void SchemaInstance::mergeFrom(const SchemaInstance& other) {
    if (this == &other) return;
    std::scoped_lock lk(m_, other.m_);
    for (const auto& kv : other.schemas_) {
        if (schemas_.find(kv.first) == schemas_.end()) {
            schemas_.emplace(kv.first, kv.second);
        }
    }
}

bool SchemaInstance::hasSchema(uint32_t hash) const {
    std::lock_guard<std::mutex> lk(m_);
    return schemas_.find(hash) != schemas_.end();
}

std::vector<uint32_t> SchemaInstance::schemaHashes() const {
    std::lock_guard<std::mutex> lk(m_);
    std::vector<uint32_t> out;
    out.reserve(schemas_.size());
    for (const auto& kv : schemas_) out.push_back(kv.first);
    return out;
}