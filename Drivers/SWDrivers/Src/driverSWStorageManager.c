#include "driverSWStorageManager.h"
#include "modConfig.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/* The old ABI used config at word 0 and four SOC floats immediately after it.
 * Never overwrite that bank during migration. Reserve its entire known extent
 * in the EEPROM transfer table, including words no longer read by this firmware.
 * The new bank has a fixed SOC address, independently committed CRCs and a schema.
 * Interrupted transactions fail closed; an explicit configuration save in the
 * commissioning loop can repair them. This is not rollback to a stale config.
 */
#define LEGACY_WORDS 166u
#define CONFIG_BASE 0x1000u
#define CONFIG_WORDS_MAX 192u
#define SOC_BASE 0x1100u
#define SOC_WORDS 8u
#define META_BASE 0x1200u
#define META_WORDS 12u
#define STORAGE_MAGIC 0x454Eu
#define STORAGE_SCHEMA 1u
#define STORAGE_COMMITTED 0xA55Au
#define ADDRESS_COUNT (LEGACY_WORDS + CONFIG_WORDS_MAX + SOC_WORDS + META_WORDS)
enum { META_MAGIC, META_SCHEMA, META_CONFIG_BYTES, META_SOC_BYTES,
       META_BOARD, META_CONFIG_CRC_LO, META_CONFIG_CRC_HI,
       META_SOC_CRC_LO, META_SOC_CRC_HI, META_CONFIG_COMMIT, META_SOC_COMMIT,
       META_RESERVED };

typedef union { uint32_t align; uint8_t bytes[CONFIG_WORDS_MAX * 2]; } ConfigBuffer;
static uint16_t addressTable[ADDRESS_COUNT];
uint16_t *driverHWEEPROMVirtAddVarTab = addressTable;
bool driverSWStorageManagerConfigEmpty = false;
bool driverSWStorageManagerStateOfChargeEmpty = false;
uint16_t driverSWStorageManagerDataSize;
uint16_t driverSWStorageManagerConfigStructSize;
uint16_t driverSWStorageManagerStateOfChargeStructSize;
static ConfigBuffer storedConfig;
static float storedSOC[4];
static bool hardwareReady, configValid, socValid;
static driverSWStorageStatus storageStatus;

typedef char checkConfigFits[(sizeof(modConfigGeneralConfigStructTypedef) <= CONFIG_WORDS_MAX * 2 &&
                             sizeof(modConfigGeneralConfigStructTypedef) % 2 == 0) ? 1 : -1];
typedef char checkPageFits[(ADDRESS_COUNT < (PAGE_SIZE / 4 - 1)) ? 1 : -1];

static uint32_t checksum(const void *data, size_t length) {
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFu;
    while (length--) {
        crc ^= *bytes++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
    }
    return ~crc;
}

static uint16_t boardTag(void) {
    return (uint16_t)checksum(HW_NAME, sizeof(HW_NAME) - 1);
}

static bool knownAddress(uint16_t address) {
    return address < LEGACY_WORDS ||
           (address >= CONFIG_BASE && address < CONFIG_BASE + CONFIG_WORDS_MAX) ||
           (address >= SOC_BASE && address < SOC_BASE + SOC_WORDS) ||
           (address >= META_BASE && address < META_BASE + META_WORDS);
}

/* Check before HAL init, which otherwise formats unrecognized page states and
 * can silently erase unknown layouts during its recovery page transfer. */
static bool pageLayoutRecognized(void) {
    uintptr_t bases[2] = { PAGE0_BASE_ADDRESS, PAGE1_BASE_ADDRESS };
    uint16_t states[2];
    for (unsigned page = 0; page < 2; ++page) {
        states[page] = *(volatile const uint16_t *)bases[page];
        if (states[page] != ERASED && states[page] != VALID_PAGE && states[page] != RECEIVE_DATA)
            return false;
        for (size_t offset = 6; offset < PAGE_SIZE; offset += 4) {
            uint16_t address = *(volatile const uint16_t *)(bases[page] + offset);
            if (address != ERASED && !knownAddress(address)) return false;
        }
    }
    if (states[0] == states[1] && states[0] != ERASED) return false;
    /* ERASED headers with programmed body are not a fresh/recognized image. */
    if (states[0] == ERASED && states[1] == ERASED) {
        for (unsigned page = 0; page < 2; ++page)
            for (size_t offset = 2; offset < PAGE_SIZE; offset += 2)
                if (*(volatile const uint16_t *)(bases[page] + offset) != ERASED) return false;
    }
    return true;
}

static bool readBytes(uint16_t base, void *data, size_t bytes) {
    uint8_t *out = data;
    for (size_t i = 0; i < bytes / 2; ++i) {
        uint16_t value;
        if (driverHWEEPROMReadVariable(base + i, &value) != HAL_OK) return false;
        memcpy(out + 2 * i, &value, 2);
    }
    return true;
}

static bool writeWord(uint16_t address, uint16_t value) {
    uint16_t check;
    if (driverHWEEPROMWriteVariable(address, value) != HAL_OK ||
        driverHWEEPROMReadVariable(address, &check) != HAL_OK || check != value) {
        storageStatus = STORAGE_STATUS_IO_ERROR;
        return false;
    }
    return true;
}

static bool writeBytes(uint16_t base, const void *data, size_t bytes) {
    const uint8_t *in = data;
    for (size_t i = 0; i < bytes / 2; ++i) {
        uint16_t value;
        memcpy(&value, in + 2 * i, 2);
        if (!writeWord(base + i, value)) return false;
    }
    return true;
}

static bool validSOC(const void *data) {
    float soc[4];
    memcpy(soc, data, sizeof(soc));
    return isfinite(soc[0]) && soc[0] >= 0 && soc[0] <= 100 &&
           isfinite(soc[1]) && soc[1] >= 0 && soc[1] <= 100 &&
           isfinite(soc[2]) && soc[2] >= 0 && soc[2] <= 1000000 &&
           isfinite(soc[3]) && soc[3] >= 0 && soc[3] <= 1000000000;
}

static bool initializeMetadata(void) {
    return writeWord(META_BASE + META_MAGIC, STORAGE_MAGIC) &&
           writeWord(META_BASE + META_CONFIG_COMMIT, 0) &&
           writeWord(META_BASE + META_SOC_COMMIT, 0) &&
           writeWord(META_BASE + META_SCHEMA, STORAGE_SCHEMA) &&
           writeWord(META_BASE + META_CONFIG_BYTES, sizeof(modConfigGeneralConfigStructTypedef)) &&
           writeWord(META_BASE + META_SOC_BYTES, sizeof(storedSOC)) &&
           writeWord(META_BASE + META_BOARD, boardTag());
}

static bool storeRecord(const void *data, StorageLocationTypedef location) {
    bool config = location == STORAGE_CONFIG;
    uint16_t base = config ? CONFIG_BASE : SOC_BASE;
    uint16_t commit = META_BASE + (config ? META_CONFIG_COMMIT : META_SOC_COMMIT);
    uint16_t crcAddress = META_BASE + (config ? META_CONFIG_CRC_LO : META_SOC_CRC_LO);
    size_t bytes = config ? sizeof(modConfigGeneralConfigStructTypedef) : sizeof(storedSOC);
    uint32_t crc = checksum(data, bytes);
    if (config) configValid = false; else socValid = false;
    return writeWord(commit, 0) && writeBytes(base, data, bytes) &&
           writeWord(crcAddress, (uint16_t)crc) && writeWord(crcAddress + 1, crc >> 16) &&
           writeWord(commit, STORAGE_COMMITTED);
}

static bool loadVersioned(void) {
    uint16_t meta[META_WORDS];
    /* Reserved word intentionally absent. */
    if (!readBytes(META_BASE, meta, (META_WORDS - 1) * 2) ||
        meta[META_MAGIC] != STORAGE_MAGIC || meta[META_SCHEMA] != STORAGE_SCHEMA ||
        meta[META_CONFIG_BYTES] != sizeof(modConfigGeneralConfigStructTypedef) ||
        meta[META_SOC_BYTES] != sizeof(storedSOC) || meta[META_BOARD] != boardTag() ||
        meta[META_CONFIG_COMMIT] != STORAGE_COMMITTED || meta[META_SOC_COMMIT] != STORAGE_COMMITTED ||
        !readBytes(CONFIG_BASE, storedConfig.bytes, sizeof(modConfigGeneralConfigStructTypedef)) ||
        !readBytes(SOC_BASE, storedSOC, sizeof(storedSOC))) return false;
    uint32_t configCRC = meta[META_CONFIG_CRC_LO] | ((uint32_t)meta[META_CONFIG_CRC_HI] << 16);
    uint32_t socCRC = meta[META_SOC_CRC_LO] | ((uint32_t)meta[META_SOC_CRC_HI] << 16);
    return checksum(storedConfig.bytes, sizeof(modConfigGeneralConfigStructTypedef)) == configCRC &&
           checksum(storedSOC, sizeof(storedSOC)) == socCRC && validSOC(storedSOC) &&
           modConfigValidate((modConfigGeneralConfigStructTypedef *)storedConfig.bytes);
}

bool driverSWStorageManagerInit(void) {
    hardwareReady = configValid = socValid = false;
    storageStatus = STORAGE_STATUS_INVALID;
    driverSWStorageManagerConfigEmpty = driverSWStorageManagerStateOfChargeEmpty = false;
    if (driverSWStorageManagerConfigStructSize * 2 != sizeof(modConfigGeneralConfigStructTypedef) ||
        driverSWStorageManagerStateOfChargeStructSize != SOC_WORDS || !pageLayoutRecognized()) return false;
    unsigned n = 0;
    for (unsigned i = 0; i < LEGACY_WORDS; ++i) addressTable[n++] = i;
    for (unsigned i = 0; i < CONFIG_WORDS_MAX; ++i) addressTable[n++] = CONFIG_BASE + i;
    for (unsigned i = 0; i < SOC_WORDS; ++i) addressTable[n++] = SOC_BASE + i;
    for (unsigned i = 0; i < META_WORDS; ++i) addressTable[n++] = META_BASE + i;
    driverSWStorageManagerDataSize = n;
    /* The transfer table MUST already be populated when EEPROM recovery runs. */
    if (HAL_FLASH_Unlock() != HAL_OK || driverHWEEPROMInit(n) != HAL_OK) {
        storageStatus = STORAGE_STATUS_IO_ERROR;
        return false;
    }
    hardwareReady = true;
    uint16_t value;
    bool versioned = false;
    for (unsigned i = LEGACY_WORDS; i < n; ++i)
        if (driverHWEEPROMReadVariable(addressTable[i], &value) == HAL_OK) versioned = true;
    if (versioned) {
        if (!loadVersioned()) return false;
        configValid = socValid = true;
        storageStatus = STORAGE_STATUS_READY;
        return true;
    }
    unsigned words = 0;
    bool missing = false;
    for (unsigned i = 0; i < LEGACY_WORDS; ++i) {
        uint16_t result = driverHWEEPROMReadVariable(i, &value);
        if (result == 1) { missing = true; continue; }
        if (result != HAL_OK) { storageStatus = STORAGE_STATUS_IO_ERROR; return false; }
        if (missing) return false; /* A partial/gapped record is not a known ABI. */
        ++words;
    }
    if (!words) { storageStatus = STORAGE_STATUS_EMPTY; return false; }
    if (words != 164 && words != 166) return false;
    /* A torn 316-byte config+SOC write can look exactly like the 312-byte ABI.
     * Never guess from byte count or plausible floats. Operator selection is
     * required for ANY unversioned image; the original bank remains untouched. */
    storageStatus = STORAGE_STATUS_LEGACY;
    return false;
}

bool driverSWStorageManagerMigrateLegacy(uint16_t configBytes) {
    if (!hardwareReady || storageStatus != STORAGE_STATUS_LEGACY ||
        (configBytes != 312 && configBytes != 316)) return false;
    uint8_t legacy[LEGACY_WORDS * 2];
    uint16_t bytes = configBytes + sizeof(storedSOC), value;
    if (!readBytes(0, legacy, bytes)) return false;
    for (unsigned i = bytes / 2; i < LEGACY_WORDS; ++i)
        if (driverHWEEPROMReadVariable(i, &value) != 1) return false;
    memcpy(storedSOC, legacy + configBytes, sizeof(storedSOC));
    if (!validSOC(storedSOC) ||
        !modConfigMigrateLegacy((modConfigGeneralConfigStructTypedef *)storedConfig.bytes,
                               legacy, configBytes)) return false;
    if (!initializeMetadata() || !storeRecord(storedConfig.bytes, STORAGE_CONFIG) ||
        !storeRecord(storedSOC, STORAGE_STATEOFCHARGE)) return false;
    configValid = socValid = true;
    storageStatus = STORAGE_STATUS_MIGRATED;
    return true;
}

bool driverSWStorageManagerIsReady(void) { return configValid && socValid; }
driverSWStorageStatus driverSWStorageManagerGetStatus(void) { return storageStatus; }

bool driverSWStorageManagerEraseData(void) {
    /* Only explicit callers may erase; operation invalidates all cached data. */
    hardwareReady = configValid = socValid = false;
    storageStatus = STORAGE_STATUS_EMPTY;
    if (driverHWEEPROMEraseFlash() != HAL_OK) { storageStatus = STORAGE_STATUS_IO_ERROR; return false; }
    return true;
}

bool driverSWStorageManagerStoreStruct(void *data, StorageLocationTypedef location) {
    if (!hardwareReady || !data || (location != STORAGE_CONFIG && location != STORAGE_STATEOFCHARGE)) return false;
    if (location == STORAGE_CONFIG) {
        if (!modConfigValidate(data)) return false;
        if (!configValid) {
            /* An explicit commissioning save starts a new schema. No unsafe
             * default pack configuration is ever loaded automatically. */
            if (!initializeMetadata()) return false;
            if (!socValid) { storedSOC[0] = 0; storedSOC[1] = 100; storedSOC[2] = storedSOC[3] = 0; }
            if (!storeRecord(storedSOC, STORAGE_STATEOFCHARGE)) return false;
            socValid = true;
        }
        if (!storeRecord(data, location)) return false;
        memcpy(storedConfig.bytes, data, sizeof(modConfigGeneralConfigStructTypedef));
        configValid = true;
    } else {
        if (!configValid || !validSOC(data) || !storeRecord(data, location)) return false;
        memcpy(storedSOC, data, sizeof(storedSOC));
        socValid = true;
    }
    storageStatus = STORAGE_STATUS_READY;
    return true;
}

bool driverSWStorageManagerGetStruct(void *data, StorageLocationTypedef location) {
    if (!data || !driverSWStorageManagerIsReady()) return false;
    if (location == STORAGE_CONFIG) { memcpy(data, storedConfig.bytes, sizeof(modConfigGeneralConfigStructTypedef)); return true; }
    if (location == STORAGE_STATEOFCHARGE) { memcpy(data, storedSOC, sizeof(storedSOC)); return true; }
    return false;
}

uint16_t driverSWStorageManagerGetOffsetFromLocation(StorageLocationTypedef location) {
    return location == STORAGE_CONFIG ? CONFIG_BASE : location == STORAGE_STATEOFCHARGE ? SOC_BASE : UINT16_MAX;
}
uint16_t driverSWStorageManagerGetStructSize(StorageLocationTypedef location) {
    return location == STORAGE_CONFIG ? driverSWStorageManagerConfigStructSize :
           location == STORAGE_STATEOFCHARGE ? driverSWStorageManagerStateOfChargeStructSize : 0;
}
