#pragma once

#include <SDK/archetyped/schema/SchemaRegistryPayload.h>
#include <SDK/archetyped/schema/SchemaBlock.h>
#include "engine/FURCMD/FURCMD.h"
#include <cstdint>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

// Thin FURCMD wrappers over SchemaFactory.
// These accept FURCMDPacket&, unpack payload and delegate to the manager.
// Registration in FractalKernel is NOT done here yet.

void schemaRegisterSchemaCMD(FURCMDPacket& packet);
void schemaGetSchemaFieldOffsetCMD(FURCMDPacket& packet);
void schemaDumpSchemaCMD(FURCMDPacket& packet);

#ifdef __cplusplus
}
#endif