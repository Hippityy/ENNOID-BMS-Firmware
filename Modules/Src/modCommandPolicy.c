#include "modCommandPolicy.h"
#include "modConfigWire.h"
#include <string.h>

static bool safeTerminal(const uint8_t *payload, size_t length) {
    static const char *const names[] = { "ping", "storage_status", "can_status", "storage_migrate312", "storage_migrate316" };
    for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
        if (length == strlen(names[i]) && !memcmp(payload, names[i], length)) return true;
    return false;
}

bool modCommandPolicyAllows(uint8_t command, const uint8_t *payload,
                            size_t length, bool commissioning) {
    if (!payload && length) return false;
    if (commissioning) {
        switch (command) {
        case COMM_FW_VERSION: case COMM_EBMS_GET_MCCONF:
        case COMM_EBMS_GET_MCCONF_DEFAULT: case COMM_EBMS_SET_MCCONF:
        case COMM_EBMS_STORE_CONF: case COMM_EBMS_GET_VALUES: case COMM_REBOOT:
            break;
        case COMM_TERMINAL_CMD:
            return safeTerminal(payload, length);
        default:
            return false;
        }
    }
    switch (command) {
    case COMM_FW_VERSION: case COMM_JUMP_TO_BOOTLOADER:
    case COMM_EBMS_GET_CELLS: case COMM_EBMS_GET_AUX: case COMM_EBMS_GET_EXP_TEMP:
    case COMM_EBMS_GET_MCCONF: case COMM_EBMS_GET_MCCONF_DEFAULT:
    case COMM_EBMS_STORE_CONF: case COMM_EBMS_GET_VALUES:
    case COMM_REBOOT: case COMM_ALIVE: case COMM_BMS_GET_VALUES:
        return length == 0;
    case COMM_EBMS_SET_MCCONF:
        return length == MOD_CONFIG_WIRE_SIZE;
    case COMM_ERASE_NEW_APP:
        return length == 4;
    case COMM_WRITE_NEW_APP_DATA:
        return length >= 4;
    case COMM_FORWARD_CAN:
        return length >= 1;
    case COMM_TERMINAL_CMD:
        // Terminal parser needs a private bounded NUL-terminated copy.
        return length > 0 && length <= 127 && !memchr(payload, '\0', length);
    case COMM_GET_CUSTOM_CONFIG: case COMM_GET_CUSTOM_CONFIG_DEFAULT:
        return length == 1;
    case COMM_SET_CUSTOM_CONFIG:
        return length == 47; // index1 + main_config_t wire46 (including signature)
    case COMM_GET_CUSTOM_CONFIG_XML:
        return length == 9;
    // Unverified force/override/reset/calibration controls remain disabled.
    default:
        return false;
    }
}
