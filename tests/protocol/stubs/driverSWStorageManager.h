#ifndef TEST_PROTOCOL_STORAGE_H
#define TEST_PROTOCOL_STORAGE_H
#include <stdint.h>
#include <stdbool.h>
typedef enum { STORAGE_CONFIG = 0, STORAGE_STATEOFCHARGE } StorageLocationTypedef;
extern bool driverSWStorageManagerConfigEmpty;
extern uint16_t driverSWStorageManagerConfigStructSize;
bool driverSWStorageManagerStoreStruct(void *, StorageLocationTypedef);
bool driverSWStorageManagerGetStruct(void *, StorageLocationTypedef);
#endif
