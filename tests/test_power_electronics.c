/* Links the production modPowerElectronics.c. Only GPIO and time are mocked. */
#include "modPowerElectronics.h"
#include "modCellMap.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

extern modPowerElectronicsPackStateTypedef *modPowerElectronicsPackStateHandle;
extern modConfigGeneralConfigStructTypedef *modPowerElectronicsGeneralConfigHandle;
extern bool modPowerElectronicsAllowForcedOnState;
static modPowerElectronicsPackStateTypedef pack;
static modConfigGeneralConfigStructTypedef config;
static bool pins[NoOfSwitches];
static unsigned balanceWrites;
void driverSWLTC6804Init(driverLTC6804ConfigStructTypedef c, uint8_t count, uint8_t cells, uint8_t temps, uint8_t type) {
    (void)c; (void)cells; (void)temps; (void)type;
    assert(count == config.cellMonitorICCount);
}
void driverSWLTC6804EnableBalanceResistorsArray(uint32_t *masks, uint8_t type) {
    (void)type;
    for (unsigned i = 0; i < NoOfCellMonitorsPossibleOnBMS; i++) assert(masks[i] == 0);
    balanceWrites++;
}

uint32_t HAL_GetTick(void) { return 10000; }
uint8_t modDelayTick1ms(uint32_t *last, uint32_t ticks) { (void)last; (void)ticks; return 1; }
void driverHWSwitchesDisableAll(void) { memset(pins, 0, sizeof(pins)); pins[SWITCH_SAFETY_OUTPUT] = true; }
void driverHWSwitchesSetSwitchState(driverHWSwitchesIDTypedef pin, driverHWSwitchesStateTypedef state) {
    assert(pin < NoOfSwitches);
    pins[pin] = state;
}

static void reset(void) {
    memset(&pack, 0, sizeof(pack));
    memset(&config, 0, sizeof(config));
    memset(pins, 0, sizeof(pins));
    modPowerElectronicsPackStateHandle = &pack;
    modPowerElectronicsGeneralConfigHandle = &config;
    pack.packOperationalCellState = PACK_STATE_NORMAL;
    pack.disChargeLCAllowed = pack.chargeAllowed = true;
    config.noOfCellsSeries = 28;
    config.noOfParallelModules = 2;
    config.cellMonitorICCount = 6;
    config.cellMonitorType = CELL_MON_LTC6811_1;
    config.firstICNoCells = 8;
    config.lastICNoOfCells = 10;
    config.maxAllowedCurrent = 100;
    config.maxAllowedChargeCurrent = 20;
    modPowerElectronicsAllowForcedOnState = false;
}

static void check_real_voltage_and_bleed_mapping(void) {
    reset();
    for (uint8_t ic = 0; ic < 6; ic++)
        for (uint8_t channel = 0; channel < 18; channel++)
            pack.cellModuleVoltages[ic][channel] = 100 * ic + channel;
    modPowerElectronicsCellMonitorsArrayTranslate();
    assert(pack.cellVoltagesIndividual[8].cellVoltage == 100);
    assert(pack.cellVoltagesIndividual[28].cellVoltage == 300);
    assert(pack.cellVoltagesIndividual[36].cellVoltage == 400);
    assert(pack.cellVoltagesIndividual[55].cellVoltage == 509);
    for (uint8_t i = 0; i < 56; i++) assert(pack.cellVoltagesIndividual[i].cellNumber == i);
    pack.cellVoltagesIndividual[8].cellBleedActive = true;
    pack.cellVoltagesIndividual[36].cellBleedActive = true;
    modPowerElectronicsCallMonitorsCalcBalanceResistorArray();
    assert(pack.cellModuleBalanceResistorEnableMask[0] == 0);
    assert(pack.cellModuleBalanceResistorEnableMask[1] == 1);
    assert(pack.cellModuleBalanceResistorEnableMask[4] == 1);

    config.noOfCellsSeries = 24;
    config.noOfParallelModules = 1;
    config.cellMonitorICCount = 3;
    config.firstICNoCells = config.lastICNoOfCells = 6;
    modPowerElectronicsCellMonitorsArrayTranslate();
    assert(pack.cellVoltagesIndividual[5].cellVoltage == 5);
    assert(pack.cellVoltagesIndividual[6].cellVoltage == 100);
    assert(pack.cellVoltagesIndividual[17].cellVoltage == 111);
    assert(pack.cellVoltagesIndividual[18].cellVoltage == 200);
    assert(pack.cellVoltagesIndividual[23].cellVoltage == 205);

    config.cellMonitorICCount = 0;
    pins[SWITCH_DISCHARGE] = true;
    modPowerElectronicsCellMonitorsArrayTranslate();
    assert(pack.faultState == FAULT_CODE_CONFIG_INVALID);
    assert(!pack.chargeAllowed && !pack.disChargeLCAllowed && !pins[SWITCH_DISCHARGE]);
}

static void check_current_limits(void) {
    const float safe[] = { -100, 0, 20 };
    const float unsafe[] = { -100.1f, 20.1f, NAN, INFINITY, -INFINITY };
    for (unsigned i = 0; i < sizeof(safe) / sizeof(*safe); i++) {
        reset(); pack.packCurrent = safe[i];
        modPowerElectronicsSubTaskCurrentWatch();
        assert(pack.packOperationalCellState == PACK_STATE_NORMAL);
    }
    for (unsigned i = 0; i < sizeof(unsafe) / sizeof(*unsafe); i++) {
        reset(); pack.packCurrent = unsafe[i];
        pack.disChargeDesired = pack.chargeDesired = pack.preChargeDesired = true;
        memset(pins, 1, sizeof(pins));
        modPowerElectronicsAllowForcedOnState = true;
        modPowerElectronicsSubTaskCurrentWatch();
        assert(pack.packOperationalCellState == PACK_STATE_ERROR_OVER_CURRENT);
        assert(pack.faultState == FAULT_CODE_OVER_CURRENT);
        assert(!pack.chargeAllowed && !pack.disChargeLCAllowed);
        for (uint8_t pin = 0; pin < NoOfSwitches; pin++)
            assert(pins[pin] == (pin == SWITCH_SAFETY_OUTPUT));
        pack.packCurrent = 0;
        modPowerElectronicsSubTaskVoltageWatch();
        assert(pack.packOperationalCellState == PACK_STATE_ERROR_OVER_CURRENT);
        assert(pack.faultState == FAULT_CODE_OVER_CURRENT);
        modPowerElectronicsAllowForcedOnState = true;
        modPowerElectronicsUpdateSwitches();
        assert(!pins[SWITCH_DISCHARGE]);
    }
}

static void check_all_module_temperatures(void) {
    reset();
    config.cellMonitorICCount = 2;
    config.noOfTempSensorPerModule = 2;
    config.tempEnableMaskBattery = 1;
    pack.auxVoltagesIndividual[0].auxVoltage = 20;
    pack.auxVoltagesIndividual[1].auxVoltage = 99; /* disabled channel */
    pack.auxVoltagesIndividual[2].auxVoltage = 70;
    pack.auxVoltagesIndividual[3].auxVoltage = -99;
    config.noOfExpansionBoard = 2;
    config.noOfTempSensorPerExpansionBoard = 1;
    config.tempEnableMaskExpansion = 1;
    pack.expVoltagesIndividual[0].expVoltage = 10;
    pack.expVoltagesIndividual[1].expVoltage = 90;
    modPowerElectronicsCalcTempStats();
    assert(pack.tempBatteryHigh == 90 && pack.tempBatteryLow == 10);
    assert(fabsf(pack.tempBatteryAverage - 47.5f) < 0.001f);
    config.tempEnableMaskBMS = 0xFFFF;
    modPowerElectronicsCalcTempStats(); /* sanitizer checks the former 14/15 overflow */
}

int main(void) {
    reset();
    modPowerElectronicsEmergencyStop();
    assert(balanceWrites == 0 && pins[SWITCH_SAFETY_OUTPUT]);
    modPowerElectronicsCellMonitorsInit();
    pack.cellModuleBalanceResistorEnableMask[0] = 1;
    pack.cellVoltagesIndividual[0].cellBleedActive = true;
    pack.balanceActive = true;
    modPowerElectronicsEmergencyStop();
    assert(balanceWrites == 1 && !pack.cellVoltagesIndividual[0].cellBleedActive && !pack.balanceActive);
    check_real_voltage_and_bleed_mapping();
    check_current_limits();
    check_all_module_temperatures();
    puts("power electronics: production mapping, current protection and temperature tests passed");
    return 0;
}
