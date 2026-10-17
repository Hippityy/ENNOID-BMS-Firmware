#include "modConfigWire.h"
#include "libBuffer.h"
#include "modCommandPolicy.h"
#include "driverSWStorageManager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
_Static_assert(COMM_EBMS_STORE_CONF == 240, "V6.05+ command IDs");
_Static_assert(COMM_EBMS_GET_VALUES == 247, "V6.05+ command IDs");

uint16_t driverSWStorageManagerConfigStructSize;
static modConfigGeneralConfigStructTypedef saved;
static bool storeOK = true;
bool driverSWStorageManagerStoreStruct(void *data, StorageLocationTypedef location) {
    assert(location == STORAGE_CONFIG);
    if (!storeOK) return false;
    saved = *(modConfigGeneralConfigStructTypedef *)data;
    return true;
}

static void unchanged_failure(modConfigGeneralConfigStructTypedef *config,
                              const uint8_t *data, size_t len) {
    modConfigGeneralConfigStructTypedef before = *config;
    assert(!modConfigWireDecode(config, data, len));
    assert(!memcmp(&before, config, sizeof(before)));
}

int main(int argc, char **argv) {
    assert(argc == 2);
    uint8_t fixture[MOD_CONFIG_WIRE_SIZE], out[MOD_CONFIG_WIRE_SIZE + 2];
    FILE *f = fopen(argv[1], "rb"); assert(f);
    assert(fread(fixture, 1, sizeof(fixture), f) == sizeof(fixture)); fclose(f);
    modConfigGeneralConfigStructTypedef config;
    memset(&config, 0xA5, sizeof(config));
    modConfigLoadDefaultConfig(&config);
    assert(modConfigValidate(&config));
    assert(config.firstICNoCells == 12 && config.lastICNoOfCells == 12);
    assert(config.shuntLCFactor < 0.0f); // signed calibration is valid
    assert(config.loadCANProtocol == loadCANProtocolNone);
    assert(config.chargerCANProtocol == chargerCANProtocolNone);
    assert(config.cellTypeUsed == 0);
    assert(modConfigWireDecode(&config, fixture, sizeof(fixture)));
    assert(config.maxAllowedCurrent == 1000.0f);
    assert(config.maxAllowedChargeCurrent == 100.0f);
    assert(config.loadCANProtocol == loadCANProtocolVESC);
    assert(config.chargerCANProtocol == chargerCANProtocolNone);
    assert(config.emitStatusOverCAN && config.emitStatusProtocol == canEmitProtocolVESC);
    memset(out, 0xA5, sizeof(out));
    assert(modConfigWireEncode(out + 1, MOD_CONFIG_WIRE_SIZE, &config) == MOD_CONFIG_WIRE_SIZE);
    assert(out[0] == 0xA5 && out[sizeof(out)-1] == 0xA5);
    assert(!memcmp(out + 1, fixture, sizeof(fixture)));
    assert(modConfigWireEncode(out, MOD_CONFIG_WIRE_SIZE-1, &config) == 0);
    for (size_t n = 0; n < MOD_CONFIG_WIRE_SIZE; ++n) unchanged_failure(&config, fixture, n);
    unchanged_failure(&config, out, MOD_CONFIG_WIRE_SIZE + 1);
    uint8_t bad[MOD_CONFIG_WIRE_SIZE];
    memcpy(bad, fixture, sizeof(bad)); bad[1] = 0; // no parallel modules
    unchanged_failure(&config, bad, sizeof(bad));
    memcpy(bad, fixture, sizeof(bad)); bad[0] = 255; // exceeds physical cell topology
    unchanged_failure(&config, bad, sizeof(bad));
    memcpy(bad, fixture, sizeof(bad));
    int32_t pos = 2; libBufferAppend_uint32(bad, 0x7F800000UL, &pos);
    unchanged_failure(&config, bad, sizeof(bad)); // overflowing/nonfinite battery capacity
    // Legacy known312/316 struct layouts preserve values, new fields default conservatively.
    uint8_t legacy[316]; memcpy(legacy, &config, sizeof(legacy));
    modConfigGeneralConfigStructTypedef migrated;
    memset(&migrated, 0xA5, sizeof(migrated));
    assert(modConfigMigrateLegacy(&migrated, legacy, 316));
    assert(migrated.maxAllowedChargeCurrent == config.maxAllowedCurrent);
    assert(migrated.loadCANProtocol == loadCANProtocolVESC);
    assert(migrated.chargerCANProtocol == chargerCANProtocolNone);
    legacy[311] = config.BMSApplication;
    assert(modConfigMigrateLegacy(&migrated, legacy, 312));
    assert(migrated.cellTypeUsed == 0 && migrated.BMSApplication == config.BMSApplication);
    assert(!modConfigMigrateLegacy(&migrated, legacy, 314));
    legacy[32] = 2;
    assert(!modConfigMigrateLegacy(&migrated, legacy, 312));
    assert(modCommandPolicyAllows(COMM_FW_VERSION, NULL, 0, true));
    assert(!modCommandPolicyAllows(COMM_FW_VERSION, fixture, 1, true));
    assert(modCommandPolicyAllows(COMM_EBMS_SET_MCCONF, fixture, sizeof(fixture), true));
    assert(!modCommandPolicyAllows(COMM_EBMS_SET_MCCONF, fixture, sizeof(fixture)-1, true));
    assert(!modCommandPolicyAllows(COMM_FORWARD_CAN, fixture, 8, true));
    assert(!modCommandPolicyAllows(COMM_JUMP_TO_BOOTLOADER, NULL, 0, true));
    assert(!modCommandPolicyAllows(COMM_BMS_FORCE_BALANCE, NULL, 0, true));
    assert(!modCommandPolicyAllows(COMM_TERMINAL_CMD, (const uint8_t *)"testbms", 7, true));
    assert(modCommandPolicyAllows(COMM_TERMINAL_CMD, (const uint8_t *)"ping", 4, true));
    assert(modCommandPolicyAllows(COMM_TERMINAL_CMD, (const uint8_t *)"storage_migrate312", 18, true));
    assert(modCommandPolicyAllows(COMM_TERMINAL_CMD, (const uint8_t *)"storage_migrate316", 18, true));
    assert(!modCommandPolicyAllows(COMM_TERMINAL_CMD, (const uint8_t *)"storage_migrate316 extra", 24, true));
    assert(!modCommandPolicyAllows(COMM_TERMINAL_CMD, fixture, 128, false));
    assert(!modCommandPolicyAllows(COMM_GET_CUSTOM_CONFIG_XML, fixture, 8, false));
    assert(modCommandPolicyAllows(COMM_GET_CUSTOM_CONFIG_XML, fixture, 9, false));
    modConfigGeneralConfigStructTypedef *live = modConfigInit();
    *live = config;
    modConfigGeneralConfigStructTypedef candidate = config;
    candidate.CANID = config.CANID + 1;
    assert(modConfigStoreCandidate(&candidate));
    assert(saved.CANID == candidate.CANID && live->CANID == config.CANID);
    storeOK = false;
    assert(!modConfigStoreCandidate(&candidate));
    assert(!memcmp(live, &config, sizeof(config)));
    puts("production config codec: fixture, atomic rejection, bounds, version IDs and legacy conversion passed");
}
