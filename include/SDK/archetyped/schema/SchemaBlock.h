#pragma once
#include <cstdint>

// Unified schema types (compatible with prototype, JVM, RealmX)
typedef enum SchemaType : uint8_t {
    SCHEMA_VOID = 0,
    SCHEMA_BOOL,
    SCHEMA_U8,
    SCHEMA_U16,
    SCHEMA_U32,
    SCHEMA_U64,
    SCHEMA_I8,
    SCHEMA_I16,
    SCHEMA_I32,
    SCHEMA_I64,
    SCHEMA_F32,
    SCHEMA_F64,
    SCHEMA_PTR,
    SCHEMA_HANDLE,
    SCHEMA_STRUCT
} SchemaType;

typedef struct FieldSchema {
    uint32_t type_id;    // SCHEMA_* or FNV-1a of nested struct
    uint32_t offset;     // byte offset (host computes via alignof)
    uint32_t size;       // byte size
    uint32_t name_off;   // offset in strpool
    uint32_t name_len;   // name length
} FieldSchema;

typedef struct SchemaBlock {
    uint32_t hash;         // FNV-1a of struct name
    uint32_t field_count;  // field count
    uint32_t total_size;   // struct size
    uint32_t strpool_size; // strpool size
} SchemaBlock;

static inline FieldSchema* schemablock_fields(SchemaBlock* sb) {
    return (FieldSchema*)(sb + 1);
}

static inline const FieldSchema* schemablock_fields_const(const SchemaBlock* sb) {
    return (const FieldSchema*)(sb + 1);
}

static inline char* schemablock_strpool(SchemaBlock* sb) {
    return (char*)(schemablock_fields(sb) + sb->field_count);
}

static inline const char* schemablock_strpool_const(const SchemaBlock* sb) {
    return (const char*)(schemablock_fields_const(sb) + sb->field_count);
}

static inline const char* schemablock_field_name(const FieldSchema* f, const char* strpool) {
    return strpool + f->name_off;
}
