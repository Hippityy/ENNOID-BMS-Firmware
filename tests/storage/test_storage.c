/* These tests compile the production storage manager and config converter.
 * The mock models persistent virtual words and page records, with power loss at
 * each individual halfword write. It does not model the HAL flash page-transfer
 * algorithm; that needs target fault-injection testing separately. */
#include "driverSWStorageManager.h"
#include "modConfig.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

uint16_t testFlash[2][PAGE_SIZE / 2];
static uint16_t values[0x1300];
static unsigned char present[0x1300];
static int failAfter = -1, writes, initCalls, failInit, failUnlock;
extern uint16_t *driverHWEEPROMVirtAddVarTab;
static void image(void) {
    memset(testFlash, 0xFF, sizeof(testFlash));
    testFlash[0][0] = VALID_PAGE;
    unsigned offset = 2;
    for (unsigned i = 0; i < sizeof(present); ++i) if (present[i]) {
        assert(offset + 1 < PAGE_SIZE / 2);
        testFlash[0][offset++] = values[i];
        testFlash[0][offset++] = i;
    }
}
static void reset(void) {
    memset(present, 0, sizeof(present));
    memset(values, 0, sizeof(values));
    memset(testFlash, 0xFF, sizeof(testFlash));
    writes = initCalls = 0;
    failAfter = -1;
    failInit = failUnlock = 0;
    driverSWStorageManagerConfigStructSize = sizeof(modConfigGeneralConfigStructTypedef) / 2;
    driverSWStorageManagerStateOfChargeStructSize = 8;
}
uint16_t HAL_FLASH_Unlock(void) { return failUnlock ? HAL_ERROR : HAL_OK; }
uint16_t driverHWEEPROMInit(uint16_t count) {
    ++initCalls;
    if (failInit) return HAL_ERROR;
    assert(count > 330 && count < 511);
    for (unsigned i = 0; i < count; ++i) {
        assert(driverHWEEPROMVirtAddVarTab[i] != 0xFFFF);
        if (i) assert(driverHWEEPROMVirtAddVarTab[i] > driverHWEEPROMVirtAddVarTab[i - 1]);
    }
    image();
    return HAL_OK;
}
uint16_t driverHWEEPROMReadVariable(uint16_t address, uint16_t *value) {
    if (address >= sizeof(present) || !present[address]) return 1;
    *value = values[address];
    return HAL_OK;
}
uint16_t driverHWEEPROMWriteVariable(uint16_t address, uint16_t value) {
    if (failAfter >= 0 && writes >= failAfter) return HAL_ERROR;
    ++writes;
    assert(address < sizeof(present));
    values[address] = value;
    present[address] = 1;
    image();
    return HAL_OK;
}
uint16_t driverHWEEPROMEraseFlash(void) { reset(); return HAL_OK; }

static modConfigGeneralConfigStructTypedef config(void) {
    modConfigGeneralConfigStructTypedef cfg;
    modConfigLoadDefaultConfig(&cfg);
    assert(modConfigValidate(&cfg));
    return cfg;
}
static void seedLegacy(unsigned bytes) {
    modConfigGeneralConfigStructTypedef cfg = config();
    unsigned char raw[332] = {0};
    const float soc[4] = {42, 97, 9.25f, 127.75f};
    assert(bytes == 312 || bytes == 316);
    memcpy(raw, &cfg, bytes);
    if (bytes == 312) raw[311] = cfg.BMSApplication;
    memcpy(raw + bytes, soc, sizeof(soc));
    for (unsigned i = 0; i < (bytes + sizeof(soc)) / 2; ++i) {
        memcpy(&values[i], raw + 2 * i, 2);
        present[i] = 1;
    }
    image();
}
static bool startLegacy(unsigned bytes) {
    assert(!driverSWStorageManagerInit());
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_LEGACY);
    return driverSWStorageManagerMigrateLegacy(bytes);
}
static void assertSOC(void) {
    float soc[4];
    assert(driverSWStorageManagerGetStruct(soc, STORAGE_STATEOFCHARGE));
    assert(soc[0] == 42 && soc[1] == 97 && soc[2] == 9.25f && soc[3] == 127.75f);
}
static void migration(unsigned bytes) {
    reset(); seedLegacy(bytes);
    uint16_t legacy[166];
    memcpy(legacy, values, sizeof(legacy));
    assert(startLegacy(bytes));
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_MIGRATED);
    assertSOC();
    assert(!memcmp(legacy, values, sizeof(legacy)));
    modConfigGeneralConfigStructTypedef loaded;
    assert(driverSWStorageManagerGetStruct(&loaded, STORAGE_CONFIG));
    assert(loaded.noOfCellsSeries == config().noOfCellsSeries);
    assert(loaded.BMSApplication == config().BMSApplication);
    assert(loaded.maxAllowedChargeCurrent == config().maxAllowedCurrent);
    assert(loaded.chargerCANProtocol == 0);
    assert(driverSWStorageManagerInit());
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_READY);
    assertSOC();
}
static void invalidImages(void) {
    reset();
    assert(!driverSWStorageManagerInit());
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_EMPTY);
    assert(!driverSWStorageManagerConfigEmpty);
    reset(); seedLegacy(316); present[25] = 0; image();
    assert(!driverSWStorageManagerInit());
    assert(writes == 0);
    reset(); seedLegacy(316); present[166] = 1; image();
    assert(!driverSWStorageManagerInit());
    assert(initCalls == 0); /* Unknown key is refused BEFORE HAL may format. */
    reset(); testFlash[0][0] = 0x1234;
    assert(!driverSWStorageManagerInit()); assert(initCalls == 0);
    reset(); failUnlock = 1;
    assert(!driverSWStorageManagerInit()); assert(initCalls == 0);
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_IO_ERROR);
    reset(); failInit = 1;
    assert(!driverSWStorageManagerInit());
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_IO_ERROR);
    reset(); seedLegacy(316); values[0] = 0; image();
    assert(!startLegacy(316)); /* Invalid cell geometry. */
    reset(); seedLegacy(316); values[164] = 0xFFFF; values[165] = 0x7FFF; image();
    assert(!startLegacy(316)); /* NaN stored SOC Wh. */
}
static void corruptedVersioned(void) {
    reset(); seedLegacy(316); assert(startLegacy(316));
    values[0x1000 + 20] ^= 1; image();
    assert(!driverSWStorageManagerInit());
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_INVALID);
    /* Must never fall back to still-valid old unversioned data. */
    reset(); seedLegacy(316); assert(startLegacy(316));
    values[0x1201] = 99; image(); assert(!driverSWStorageManagerInit());
    reset(); seedLegacy(316); assert(startLegacy(316));
    values[0x1204] ^= 1; image(); assert(!driverSWStorageManagerInit());
}
static void explicitProvenance(void) {
    reset(); seedLegacy(312);
    assert(!driverSWStorageManagerInit());
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_LEGACY);
    assert(!driverSWStorageManagerMigrateLegacy(316));
    assert(!driverSWStorageManagerMigrateLegacy(320));
    assert(writes == 0 && !driverSWStorageManagerIsReady());
    reset(); seedLegacy(316);
    assert(!driverSWStorageManagerInit());
    assert(!driverSWStorageManagerMigrateLegacy(312)); /* No truncation of trailing SOC. */
    assert(writes == 0);
    /* A power loss after config316+12SOC leaves exactly the same extent as a
     * complete baseline312+16SOC. Boot must not infer ABI from that extent. */
    reset(); seedLegacy(316);
    present[164] = present[165] = 0; image();
    assert(!driverSWStorageManagerInit());
    assert(driverSWStorageManagerGetStatus() == STORAGE_STATUS_LEGACY);
    assert(!driverSWStorageManagerMigrateLegacy(316));
    assert(writes == 0 && !driverSWStorageManagerIsReady());
}
static void powerCuts(void) {
    /* Interrupted migration: original config and SOC are always retained; an
     * incomplete new schema is refused rather than guessing or using defaults. */
    for (int cut = 0; cut < 220; ++cut) {
        reset(); seedLegacy(312);
        uint16_t old[166]; memcpy(old, values, sizeof(old));
        failAfter = cut;
        bool migrated = startLegacy(312);
        assert(!memcmp(old, values, sizeof(old)));
        failAfter = -1;
        bool reboot = driverSWStorageManagerInit();
        if (migrated) { assert(reboot); assertSOC(); }
        else if (cut == 0) { assert(!reboot); assert(driverSWStorageManagerMigrateLegacy(312)); assertSOC(); } /* No write occurred. */
        else assert(!reboot);
    }
    for (int cut = 0; cut < 180; ++cut) {
        reset(); seedLegacy(316); assert(startLegacy(316));
        modConfigGeneralConfigStructTypedef cfg = config();
        cfg.CANID = 21;
        failAfter = cut; writes = 0;
        bool stored = driverSWStorageManagerStoreStruct(&cfg, STORAGE_CONFIG);
        failAfter = -1;
        bool reboot = driverSWStorageManagerInit();
        if (stored || cut == 0) { assert(reboot); assertSOC(); }
        else assert(!reboot);
    }
    for (int cut = 0; cut < 15; ++cut) {
        reset(); seedLegacy(316); assert(startLegacy(316));
        float soc[4] = {12, 90, 1, 10};
        failAfter = cut; writes = 0;
        bool stored = driverSWStorageManagerStoreStruct(soc, STORAGE_STATEOFCHARGE);
        failAfter = -1;
        bool reboot = driverSWStorageManagerInit();
        if (stored || cut == 0) assert(reboot);
        else assert(!reboot);
    }
}
static void commissionAndSOC(void) {
    reset(); assert(!driverSWStorageManagerInit());
    modConfigGeneralConfigStructTypedef cfg = config();
    assert(driverSWStorageManagerStoreStruct(&cfg, STORAGE_CONFIG));
    assert(driverSWStorageManagerInit());
    float soc[4]; assert(driverSWStorageManagerGetStruct(soc, STORAGE_STATEOFCHARGE));
    assert(soc[0] == 0 && soc[1] == 100 && soc[2] == 0);
    float newSOC[4] = {12, 90, 1, 10};
    assert(driverSWStorageManagerStoreStruct(newSOC, STORAGE_STATEOFCHARGE));
    assert(driverSWStorageManagerInit());
    assert(driverSWStorageManagerGetStruct(soc, STORAGE_STATEOFCHARGE));
    assert(!memcmp(soc, newSOC, sizeof(soc)));
    newSOC[0] = NAN; assert(!driverSWStorageManagerStoreStruct(newSOC, STORAGE_STATEOFCHARGE));
    assert(!driverSWStorageManagerGetStruct(soc, (StorageLocationTypedef)20));
    assert(driverSWStorageManagerEraseData()); assert(!driverSWStorageManagerIsReady());
}
int main(void) {
    migration(312); migration(316); invalidImages(); corruptedVersioned(); explicitProvenance();
    powerCuts(); commissionAndSOC();
    puts("storage tests: migration/SOC preservation, validation, commissioning, CRC, and every write interruption passed");
}
