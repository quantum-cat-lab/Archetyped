#pragma once

#include <SDK/archetyped/schema/SchemaBlock.h>
#include <cstdint>


// Register schema in domain
typedef struct SchemaRegisterSchemaPayload {
    char domain[64];
    char name[64];
    uint32_t fieldCount;
    struct {
        uint32_t typeId;
        char name[48];
    } fields[16];
} SchemaRegisterSchemaPayload;

// Query field offset
typedef struct SchemaGetSchemaFieldOffsetInstancePayload {
    char domain[64];
    uint32_t schemaHash;
    uint64_t fieldHash;
    uint32_t outOffset;
} SchemaGetSchemaFieldOffsetInstancePayload;

// Dump schema to caller buffer
typedef struct SchemaDumpSchemaInstancePayload {
    char domain[64];
    void* outputBuffer;
    uint32_t bufferSize;
    uint32_t outSize;
    uint64_t* fence;
} SchemaDumpSchemaInstancePayload;