#ifndef TEST_EEPROM_H
#define TEST_EEPROM_H
#include <stdint.h>
#include <stddef.h>
#define HAL_OK 0u
#define HAL_ERROR 1u
#define PAGE_SIZE 2048u
#define ERASED 0xFFFFu
#define RECEIVE_DATA 0xEEEEu
#define VALID_PAGE 0u
extern uint16_t testFlash[2][PAGE_SIZE / 2];
#define PAGE0_BASE_ADDRESS ((uintptr_t)testFlash[0])
#define PAGE1_BASE_ADDRESS ((uintptr_t)testFlash[1])
uint16_t HAL_FLASH_Unlock(void);
uint16_t driverHWEEPROMInit(uint16_t count);
uint16_t driverHWEEPROMReadVariable(uint16_t address, uint16_t *value);
uint16_t driverHWEEPROMWriteVariable(uint16_t address, uint16_t value);
uint16_t driverHWEEPROMEraseFlash(void);
#endif
