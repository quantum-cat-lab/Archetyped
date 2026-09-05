#include "SchemaRegistryCMD.h"
#include <SDK/archetyped/schema/SchemaBlock.h>
#include "SchemaFactory.h"
#include <SDK/archetyped/hash/hash.h>
#include <cstring>
#include <cstdio>

void schemaRegisterSchemaCMD(FURCMDPacket& packet) {
    auto* p = static_cast<SchemaRegisterSchemaPayload*>(packet.payload);
    if (!p || !packet.payloadSize) return;

    std::vector<std::pair<uint32_t, std::string>> fields;
    fields.reserve(p->fieldCount);
    for (uint32_t i = 0; i < p->fieldCount; ++i) {
        fields.emplace_back(p->fields[i].typeId, p->fields[i].name);
    }

    SchemaFactory::instance().registerSchema(p->domain, p->name, fields);
    std::fprintf(stderr, "[SchemaInstance] domain='%s' schema='%s' fields=%u\n", p->domain, p->name, p->fieldCount);
}

void schemaGetSchemaFieldOffsetCMD(FURCMDPacket& packet) {
    auto* p = static_cast<SchemaGetSchemaFieldOffsetInstancePayload*>(packet.payload);
    if (!p || !packet.payloadSize) return;

    uint32_t offset = SchemaFactory::instance().getSchemaFieldOffset(p->domain, p->schemaHash, p->fieldHash);
    p->outOffset = offset;

    if (packet.fence) *packet.fence = 1;
}

void schemaDumpSchemaCMD(FURCMDPacket& packet) {
    auto* p = static_cast<SchemaDumpSchemaInstancePayload*>(packet.payload);
    if (!p || !packet.payloadSize || !p->outputBuffer) return;

    auto buf = SchemaFactory::instance().dumpSchema(p->domain);
    p->outSize = 0;
    if (!buf.empty() && buf.size() <= p->bufferSize) {
        std::memcpy(p->outputBuffer, buf.data(), buf.size());
        p->outSize = static_cast<uint32_t>(buf.size());
    }

    if (packet.fence) *packet.fence = 1;
}