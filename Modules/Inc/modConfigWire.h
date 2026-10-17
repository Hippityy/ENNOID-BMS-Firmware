#ifndef MOD_CONFIG_WIRE_H
#define MOD_CONFIG_WIRE_H
#include "modConfig.h"
#include <stddef.h>
#define MOD_CONFIG_WIRE_SIZE 268U
/* Length excludes command byte. Failure never changes *config. */
bool modConfigWireDecode(modConfigGeneralConfigStructTypedef *config,
                         const uint8_t *data, size_t length);
size_t modConfigWireEncode(uint8_t *data, size_t capacity,
                           const modConfigGeneralConfigStructTypedef *config);
#endif
