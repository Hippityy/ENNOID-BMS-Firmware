#ifndef __DRIVERSWSTORAGEMANAGER_H
#define __DRIVERSWSTORAGEMANAGER_H

#include "stdint.h"
#include "stdbool.h"
#include "driverHWEEPROM.h"

extern bool driverSWStorageManagerConfigEmpty;
extern bool driverSWStorageManagerStateOfChargeEmpty;
extern uint16_t driverSWStorageManagerConfigStructSize;
extern uint16_t driverSWStorageManagerStateOfChargeStructSize;

typedef enum {
	STORAGE_CONFIG = 0,
	STORAGE_STATEOFCHARGE,
} StorageLocationTypedef;

typedef enum {
	STORAGE_STATUS_NOT_INITIALIZED = 0,
	STORAGE_STATUS_READY,
	STORAGE_STATUS_MIGRATED,
	STORAGE_STATUS_EMPTY,
	STORAGE_STATUS_LEGACY,
	STORAGE_STATUS_INVALID,
	STORAGE_STATUS_IO_ERROR
} driverSWStorageStatus;

/* Failure requires commissioning with power outputs disabled, never defaults. */
bool driverSWStorageManagerInit(void);
bool driverSWStorageManagerIsReady(void);
driverSWStorageStatus driverSWStorageManagerGetStatus(void);
/* Operator must identify the previous open-source ABI (312 or 316 bytes).
 * Unversioned size/content cannot prove provenance or distinguish torn writes.
 * Use only with outputs disabled; stock/vendor layouts are not supported. */
bool driverSWStorageManagerMigrateLegacy(uint16_t configBytes);
bool driverSWStorageManagerEraseData(void);
bool driverSWStorageManagerStoreStruct(void *configStruct, StorageLocationTypedef storageLocation);
bool driverSWStorageManagerGetStruct(void *configStruct, StorageLocationTypedef storageLocation);
uint16_t driverSWStorageManagerGetOffsetFromLocation(StorageLocationTypedef storageLocation);
uint16_t driverSWStorageManagerGetStructSize(StorageLocationTypedef storageLocation);

#endif
