#include "modConfigWire.h"
#include "modConfigWireFields.h"
#include "libBuffer.h"

size_t modConfigWireEncode(uint8_t *data, size_t capacity,
                           const modConfigGeneralConfigStructTypedef *config) {
    if (!data || !config || capacity < MOD_CONFIG_WIRE_SIZE) return 0;
    int32_t ind = 0;
#define WRITE_U8(member) libBufferAppend_uint8(data, config->member, &ind)
#define WRITE_BOOL(member) WRITE_U8(member)
#define WRITE_U16(member) libBufferAppend_uint16(data, config->member, &ind)
#define WRITE_I16(member) libBufferAppend_int16(data, config->member, &ind)
#define WRITE_U32(member) libBufferAppend_uint32(data, config->member, &ind)
#define WRITE_F32(member) libBufferAppend_float32_auto(data, config->member, &ind)
#define WRITE(kind, member, wire_name) WRITE_##kind(member);
    MOD_CONFIG_WIRE_FIELDS(WRITE)
#undef WRITE
    return (size_t)ind;
}

bool modConfigWireDecode(modConfigGeneralConfigStructTypedef *config,
                         const uint8_t *data, size_t length) {
    if (!data || !config || length != MOD_CONFIG_WIRE_SIZE) return false;
    modConfigGeneralConfigStructTypedef next = *config;
    int32_t ind = 0;
#define READ_U8(member) next.member = libBufferGet_uint8(data, &ind)
#define READ_BOOL(member) do { uint8_t value = libBufferGet_uint8(data, &ind); \
    if (value > 1) return false; \
    next.member = value; } while (0)
#define READ_U16(member) next.member = libBufferGet_uint16(data, &ind)
#define READ_I16(member) next.member = libBufferGet_int16(data, &ind)
#define READ_U32(member) next.member = libBufferGet_uint32(data, &ind)
#define READ_F32(member) next.member = libBufferGet_float32_auto(data, &ind)
#define READ(kind, member, wire_name) READ_##kind(member);
    MOD_CONFIG_WIRE_FIELDS(READ)
#undef READ
    if (ind != MOD_CONFIG_WIRE_SIZE || !modConfigValidate(&next)) return false;
    modConfigSyncLegacyCAN(&next);
    modConfigPublish(config, &next);
    return true;
}
