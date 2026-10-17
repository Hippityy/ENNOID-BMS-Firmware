/*
	Copyright 2017 - 2018 Danny Bokma	danny@diebie.nl
	Copyright 2019 - 2020 Kevin Dionne	kevin.dionne@ennoid.me

	This file is part of the DieBieMS/ENNOID-BMS firmware.

	The DieBieMS/ENNOID-BMS firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The DieBieMS/ENNOID-BMS firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "modConfig.h"
#include "modCellMap.h"
#include "modConfigWireFields.h"
#include "driverSWStorageManager.h"
#include <string.h>
#include <stddef.h>
#include <math.h>

modConfigGeneralConfigStructTypedef modConfigGeneralConfig;

modConfigGeneralConfigStructTypedef* modConfigInit(void) {
	driverSWStorageManagerConfigStructSize = (sizeof(modConfigGeneralConfigStructTypedef)/sizeof(uint16_t)); // Calculate the space needed for the config struct in EEPROM
	return &modConfigGeneralConfig;
};

bool modConfigStoreAndLoadDefaultConfig(void) {
    // Unknown/empty flash requires explicit commissioning, not silent defaults.
    return modConfigLoadConfig();
}

bool modConfigStoreConfig(void) {
    modConfigGeneralConfigStructTypedef next = modConfigGeneralConfig;
    if (!modConfigValidate(&next)) return false;
    modConfigSyncLegacyCAN(&next);
    if (!driverSWStorageManagerStoreStruct(&next, STORAGE_CONFIG)) return false;
    modConfigPublish(&modConfigGeneralConfig, &next);
    return true;
}

bool modConfigStoreCandidate(const modConfigGeneralConfigStructTypedef *candidate) {
    if (!candidate) return false;
    modConfigGeneralConfigStructTypedef next = *candidate;
    if (!modConfigValidate(&next)) return false;
    modConfigSyncLegacyCAN(&next);
    /* Persist for reboot without reconfiguring the running measurement chain. */
    return driverSWStorageManagerStoreStruct(&next, STORAGE_CONFIG);
}

bool modConfigLoadConfig(void) {
    modConfigGeneralConfigStructTypedef next;
    memset(&next, 0, sizeof(next));
    if (!driverSWStorageManagerGetStruct(&next, STORAGE_CONFIG) ||
        !modConfigValidate(&next)) return false;
    modConfigSyncLegacyCAN(&next);
    modConfigPublish(&modConfigGeneralConfig, &next);
    return true;
}

bool modConfigStoreDefaultConfig(void) {
    modConfigGeneralConfigStructTypedef defaults;
    modConfigLoadDefaultConfig(&defaults);
    if (!modConfigValidate(&defaults)) return false;
    if (!driverSWStorageManagerStoreStruct(&defaults, STORAGE_CONFIG)) return false;
    driverSWStorageManagerConfigEmpty = false;
    return true;
}

bool modconfigHardwareLimitsApply(modConfigGeneralConfigStructTypedef *configLocation) {
    modCellMap map;
    if (!configLocation || configLocation->cellMonitorICCount > HW_LIM_CELL_MONITOR_IC_COUNT ||
        !modCellMapBuild(&map, configLocation->noOfCellsSeries,
                         configLocation->noOfParallelModules,
                         configLocation->cellMonitorICCount,
                         configLocation->cellMonitorType,
                         configLocation->firstICNoCells,
                         configLocation->lastICNoOfCells)) return false;

    configLocation->maxSimultaneousDischargingCells = MIN(configLocation->maxSimultaneousDischargingCells,
                                                          HW_LIM_CELL_BALANCE_MAX_SIMULTANEOUS_DISCHARGE);
    if (configLocation->notUsedTimeout)
        configLocation->notUsedTimeout = MAX(configLocation->notUsedTimeout, HW_LIM_MIN_NOT_USED_DELAY);
    configLocation->noOfCellsPerModule = 0;
    for (uint8_t ic = 0; ic < map.icCount; ic++)
        configLocation->noOfCellsPerModule = MAX(configLocation->noOfCellsPerModule, map.cellsPerIC[ic]);
    configLocation->firstICNoCells = map.cellsPerIC[0];
    configLocation->lastICNoOfCells = map.cellsPerIC[map.icsPerString - 1];
    configLocation->lastICMask = (1UL << configLocation->lastICNoOfCells) - 1;
    return true;
}

void modConfigLoadDefaultConfig(modConfigGeneralConfigStructTypedef *configLocation) {
    memset(configLocation, 0, sizeof(*configLocation));
#if ENNOID_LV
	configLocation->noOfCellsSeries					= 12;					// Total number of cells in series in the battery pack
	configLocation->noOfCellsParallel                              	= 10;       				// Number of cells in parallel
	configLocation->noOfParallelModules                       	= 1;      				// Number of parallel modules
	configLocation->batteryCapacity					= 22.00f;				// XXAh battery
	configLocation->cellHardUnderVoltage				= 2.30f;				// Worst case X.XXV as lowest cell voltage
	configLocation->cellHardOverVoltage				= 4.20f;				// Worst case X.XXV as highest cell voltage
	configLocation->cellLCSoftUnderVoltage				= 2.70f;				// Lowest cell voltage X.XXV.
	configLocation->cellSoftOverVoltage				= 4.10f;				// Normal highest cell voltage X.XXV.
	configLocation->cellBalanceDifferenceThreshold                 	= 0.005f;				// Start balancing @ XmV difference, stop if below.
	configLocation->cellBalanceStart				= 4.0f;					// Start balancing above X.XXV.
	configLocation->cellBalanceAllTime				= false;				// Enable balancing under all opstate
	configLocation->cellThrottleUpperStart				= 0.03f;				// Upper range of cell voltage for charge throttling.
	configLocation->cellThrottleLowerStart				= 0.20f;				// Lower range of cell voltage for discharge throttling.
	configLocation->cellThrottleUpperMargin				= 0.01f;				// Margin of throttle from upper soft limits.
	configLocation->cellThrottleLowerMargin				= 0.50f;				// Margin of throttle from lower soft limits.	
	configLocation->packVoltageDataSource                          	= sourcePackVoltageISL28022;		// Packvoltage source.
	configLocation->packCurrentDataSource                          	= sourcePackCurrentISL28022; 		// The pack current is the same as the current through the low current shunt
	configLocation->buzzerSignalSource                             	= buzzerSourceOn;         		// Stores what source shoud be taken to trigger
	configLocation->buzzerSignalPersistant                         	= false;                    		// Stores whether the buzzer should stay on after triggering
  	configLocation->shuntLCFactor                                  	= -0.051f;              		// Shunt factor low current
	configLocation->voltageLCFactor	                               	= 3.50f;                   		// Pack voltage factor 
	configLocation->voltageLCOffset                                	= 0;                       		// Pack voltage offset 
	configLocation->loadVoltageFactor	                       	= 49.2f;                   		// Load voltage factor 
	configLocation->loadVoltageOffset                              	= 0.0f;                    		// Load voltage offset
	configLocation->chargerVoltageFactor	                       	= 49.2f;                   		// Charger voltage factor 
	configLocation->chargerVoltageOffset                           	= 0.0f;                    		// Charger voltage offset
	configLocation->throttleChargeIncreaseRate                     	= 1;                       		// Percentage charge throttle increase rate per 100ms (cell voltage loop time)  
	configLocation->throttleDisChargeIncreaseRate                  	= 2;                       		// Percentage discharge throttle increase rate per 100ms (cell voltage loop time)  	
	configLocation->cellBalanceUpdateInterval			= 4*1000;				// Keep calculated resistors enabled for this amount of time in miliseconds.
	configLocation->maxSimultaneousDischargingCells			= 5;					// Allow a maximum of X cells simultinous discharging trough bleeding resistors.
	configLocation->timeoutDischargeRetry				= 10*1000;				// Wait for X seconds before retrying to enable load.
	configLocation->hysteresisDischarge 				= 0.02f;				// Lowest cell should rise XXmV before output is re enabled.
	configLocation->timeoutChargeRetry				= 100*1000;				// Wait for XX seconds before retrying to enable charger.
	configLocation->hysteresisCharge				= 0.01f;				// Highest cell should lower XXmV before charger is re enabled.
	configLocation->timeoutChargeCompleted				= 30*60*1000;				// Wait for XX minutes before setting charge state to charged.
	configLocation->timeoutChargingCompletedMinimalMismatch 	= 6*1000;				// If cell mismatch is under threshold and (charging is not allowed) wait this delay time to set "charged" state.
	configLocation->maxMismatchThreshold				= 0.010f;				// If mismatch is under this threshold for timeoutChargingCompletedMinimalMismatch determin fully charged.
	configLocation->chargerEnabledThreshold				= 0.5f;					// If charge current > X.XA stay in charging mode and dont power off.
	configLocation->timeoutChargerDisconnected			= 2000;					// Wait for X seconds to respond to charger disconnect.
	configLocation->minimalPrechargePercentage			= 0.70f;				// output should be at a minimal of 80% of input voltage.
	configLocation->timeoutLCPreCharge				= 1.5*1000;				// Precharge error timeout, allow 1.5 seconds pre-charge time before declaring load error.
	configLocation->maxAllowedCurrent				= 1000.0f;				// Allow max XXXA trough BMS.
	configLocation->allowedTempBattDischargingMax                  	= 75.0f;                  		// Max battery temperature where discharging is still allowed
	configLocation->allowedTempBattDischargingMin                  	= 0.0f;                    		// Min battery temperature where discharging is still allowed
	configLocation->allowedTempBattChargingMax                     	= 50.0f;                   		// Max battery temperature where charging is still allowed
	configLocation->allowedTempBattChargingMin                     	= 0.0f;                    		// Min battery temperature where charging is still allowed
	configLocation->allowedTempBattCoolingMax                      	= 5.0f;                    		// Max battery temperature where cooling is activated
	configLocation->allowedTempBattCoolingMin                      	= 50.0f;                   		// Min battery temperature where heating is activated
	configLocation->allowedTempBMSMax                              	= 80.0f;                   		// Max BMS operational temperature
	configLocation->allowedTempBMSMin                              	= 0.0f;                    		// Min BMS operational temperature
	configLocation->displayTimeoutBatteryDead			= 5000;					// Show battery dead symbol X seconds before going to powerdown in cell voltage error state.
	configLocation->displayTimeoutBatteryError			= 5000;					// Show error symbol for X seconds before going to powerdown in general error state.
	configLocation->displayTimeoutBatteryErrorPreCharge		= 10000;				// Show pre charge error for XX seconds.
	configLocation->displayTimeoutSplashScreen			= 3000;					// Display / INIT splash screen time.
	configLocation->displayStyle					= advanced;				// Display style used for showing the SSD1306 data
	configLocation->maxUnderAndOverVoltageErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->maxUnderAndOverTemperatureErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->notUsedCurrentThreshold				= 1.0f;					// If abs(packcurrent) < X.XA consider pack as not used.
	configLocation->notUsedTimeout					= 20*60*1000;				// If pack is not used for longer than XX minutes disable bms.
	configLocation->stateOfChargeStoreInterval			= 60*1000;				// Interval in ms to store state of charge information.
	configLocation->stateOfChargeMethod                            	= socCoulomb;              		// Use coulomb counting for SoC calculation
	configLocation->CANID						= 10;					// CAN ID for CAN communication.
	configLocation->CANIDStyle                                     	= CANIDStyleVESC;          		// CAN ID default Style.
	configLocation->emitStatusOverCAN                              	= false;                   		// Send status over can.
	configLocation->canBusSpeed                                    	= canSpeedBaud500k;        		// 500k CAN baud
	configLocation->emitStatusProtocol                             	= canEmitProtocolVESC; 			// Can emit protocol set to MG style for backwards compatibility
	configLocation->tempEnableMaskBMS                              	= 0x0001;				// Bitwise select what sensor to enable for the BMS (internal sensors).
	configLocation->tempEnableMaskBattery                          	= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
	configLocation->tempEnableMaskExpansion                     	= 0xFFFF;				// Bitwise select what sensor to enable for the expansion boards(external sensors).
  	configLocation->noOfTempSensorPerModule            		= 1;					// Number of temperature sensors monitored per LTC68XX
	configLocation->noOfExpansionBoard   				= 0;					// Number of expansion board
	configLocation->noOfTempSensorPerExpansionBoard          	= 0;					// Number of temperature sensors monitored per expansion board
	configLocation->LCUseDischarge                                 	= enabled;                 		// Enable or disable the solid state output
	configLocation->LCUsePrecharge                                 	= enabled;              		// Use precharge before enabling main output
	configLocation->allowChargingDuringDischarge                   	= true;                    		// Allow the battery to be charged in normal mode
	configLocation->allowForceOn                                   	= false;                   		// Allow the BMS to be forced ON by long actuation of the power button
	configLocation->pulseToggleButton                              	= true;                    		// Select either pulse or toggle power button
	configLocation->useCANSafetyInput                              	= false;                   		// Use the safety input status from CAN
	configLocation->useCANDelayedPowerDown                         	= false;                   		// Use delayed power down
	configLocation->NTCTopResistor[modConfigNTCGroupLTCExt]        	= 100000;                  		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupMasterPCB]     	= 100000;                  		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupExp]  		= 100000;                  		// NTC Pullup resistor value
	configLocation->NTC25DegResistance[modConfigNTCGroupLTCExt]    	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupMasterPCB] 	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupExp] 	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTCBetaFactor[modConfigNTCGroupLTCExt]         	= 4250;                  		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupMasterPCB]      	= 4250;                    		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupExp]      	= 4250;                    		// NTC Beta factor
	configLocation->cellMonitorType                                	= CELL_MON_LTC6811_1;      		// Use the new cell voltage monitor
	configLocation->cellMonitorICCount                             	= 1;                       		// Only one slave IC
	configLocation->externalEnableOperationalState                 	= opStateExtNormal;        		// Go to normal enable mode
	configLocation->chargeEnableOperationalState                   	= opStateChargingModeCharging;		// Go to charging mode when a charger is connected
	configLocation->powerDownDelay                                 	= 3000;                    		// Wait only minimal to turn off

	configLocation->noOfCellsPerModule                             	= 12;                      		// Number of cell levels monitored per LTC68XX
	configLocation->lastICNoOfCells					= 0;
	configLocation->lastICMask					= 0;
	configLocation->humidityICType 				 	= 0;
	configLocation->BMSApplication					= electricVehicle;

#elif ENNOID_LV_2
	configLocation->noOfCellsSeries					= 12;					// Total number of cells in series in the battery pack
	configLocation->noOfCellsParallel                              	= 10;       				// Number of cells in parallel
	configLocation->noOfParallelModules                       	= 1;      				// Number of parallel modules
	configLocation->batteryCapacity					= 22.00f;				// XXAh battery
	configLocation->cellHardUnderVoltage				= 2.30f;				// Worst case X.XXV as lowest cell voltage
	configLocation->cellHardOverVoltage				= 4.20f;				// Worst case X.XXV as highest cell voltage
	configLocation->cellLCSoftUnderVoltage				= 2.70f;				// Lowest cell voltage X.XXV.
	configLocation->cellSoftOverVoltage				= 4.10f;				// Normal highest cell voltage X.XXV.
	configLocation->cellBalanceDifferenceThreshold                 	= 0.005f;				// Start balancing @ XmV difference, stop if below.
	configLocation->cellBalanceStart				= 4.0f;					// Start balancing above X.XXV.
	configLocation->cellBalanceAllTime				= false;				// Enable balancing under all opstate
	configLocation->cellThrottleUpperStart				= 0.03f;				// Upper range of cell voltage for charge throttling.
	configLocation->cellThrottleLowerStart				= 0.20f;				// Lower range of cell voltage for discharge throttling.
	configLocation->cellThrottleUpperMargin				= 0.01f;				// Margin of throttle from upper soft limits.
	configLocation->cellThrottleLowerMargin				= 0.50f;				// Margin of throttle from lower soft limits.	
	configLocation->packVoltageDataSource                          	= sourcePackVoltageISL28022;		// Packvoltage source.
	configLocation->packCurrentDataSource                          	= sourcePackCurrentISL28022; 		// The pack current is the same as the current through the low current shunt
	configLocation->buzzerSignalSource                             	= buzzerSourceOn;         		// Stores what source shoud be taken to trigger
	configLocation->buzzerSignalPersistant                         	= false;                    		// Stores whether the buzzer should stay on after triggering
  	configLocation->shuntLCFactor                                  	= -0.051f;              		// Shunt factor low current
	configLocation->voltageLCFactor	                               	= 2.50f;                   		// Pack voltage factor 
	configLocation->voltageLCOffset                                	= 0;                       		// Pack voltage offset 
	configLocation->loadVoltageFactor	                       	= 90.0f;                   		// Load voltage factor 
	configLocation->loadVoltageOffset                              	= 0.0f;                    		// Load voltage offset
	configLocation->chargerVoltageFactor	                       	= 90.0f;                   		// Charger voltage factor 
	configLocation->chargerVoltageOffset                           	= 0.0f;                    		// Charger voltage offset
	configLocation->throttleChargeIncreaseRate                     	= 1;                       		// Percentage charge throttle increase rate per 100ms (cell voltage loop time)  
	configLocation->throttleDisChargeIncreaseRate                  	= 2;                       		// Percentage discharge throttle increase rate per 100ms (cell voltage loop time)  	
	configLocation->cellBalanceUpdateInterval			= 4*1000;				// Keep calculated resistors enabled for this amount of time in miliseconds.
	configLocation->maxSimultaneousDischargingCells			= 5;					// Allow a maximum of X cells simultinous discharging trough bleeding resistors.
	configLocation->timeoutDischargeRetry				= 10*1000;				// Wait for X seconds before retrying to enable load.
	configLocation->hysteresisDischarge 				= 0.02f;				// Lowest cell should rise XXmV before output is re enabled.
	configLocation->timeoutChargeRetry				= 100*1000;				// Wait for XX seconds before retrying to enable charger.
	configLocation->hysteresisCharge				= 0.01f;				// Highest cell should lower XXmV before charger is re enabled.
	configLocation->timeoutChargeCompleted				= 30*60*1000;				// Wait for XX minutes before setting charge state to charged.
	configLocation->timeoutChargingCompletedMinimalMismatch 	= 6*1000;				// If cell mismatch is under threshold and (charging is not allowed) wait this delay time to set "charged" state.
	configLocation->maxMismatchThreshold				= 0.010f;				// If mismatch is under this threshold for timeoutChargingCompletedMinimalMismatch determin fully charged.
	configLocation->chargerEnabledThreshold				= 0.5f;					// If charge current > X.XA stay in charging mode and dont power off.
	configLocation->timeoutChargerDisconnected			= 2000;					// Wait for X seconds to respond to charger disconnect.
	configLocation->minimalPrechargePercentage			= 0.70f;				// output should be at a minimal of 80% of input voltage.
	configLocation->timeoutLCPreCharge				= 1.5*1000;				// Precharge error timeout, allow 1.5 seconds pre-charge time before declaring load error.
	configLocation->maxAllowedCurrent				= 1000.0f;				// Allow max XXXA trough BMS.
	configLocation->allowedTempBattDischargingMax                  	= 75.0f;                  		// Max battery temperature where discharging is still allowed
	configLocation->allowedTempBattDischargingMin                  	= 0.0f;                    		// Min battery temperature where discharging is still allowed
	configLocation->allowedTempBattChargingMax                     	= 50.0f;                   		// Max battery temperature where charging is still allowed
	configLocation->allowedTempBattChargingMin                     	= 0.0f;                    		// Min battery temperature where charging is still allowed
	configLocation->allowedTempBattCoolingMax                      	= 5.0f;                    		// Max battery temperature where cooling is activated
	configLocation->allowedTempBattCoolingMin                      	= 50.0f;                   		// Min battery temperature where heating is activated
	configLocation->allowedTempBMSMax                              	= 80.0f;                   		// Max BMS operational temperature
	configLocation->allowedTempBMSMin                              	= 0.0f;                    		// Min BMS operational temperature
	configLocation->displayTimeoutBatteryDead			= 5000;					// Show battery dead symbol X seconds before going to powerdown in cell voltage error state.
	configLocation->displayTimeoutBatteryError			= 5000;					// Show error symbol for X seconds before going to powerdown in general error state.
	configLocation->displayTimeoutBatteryErrorPreCharge		= 10000;				// Show pre charge error for XX seconds.
	configLocation->displayTimeoutSplashScreen			= 3000;					// Display / INIT splash screen time.
	configLocation->displayStyle					= advanced;				// Display style used for showing the SSD1306 data
	configLocation->maxUnderAndOverVoltageErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->maxUnderAndOverTemperatureErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->notUsedCurrentThreshold				= 1.0f;					// If abs(packcurrent) < X.XA consider pack as not used.
	configLocation->notUsedTimeout					= 20*60*1000;				// If pack is not used for longer than XX minutes disable bms.
	configLocation->stateOfChargeStoreInterval			= 60*1000;				// Interval in ms to store state of charge information.
	configLocation->stateOfChargeMethod                            	= socCoulomb;              		// Use coulomb counting for SoC calculation
	configLocation->CANID						= 10;					// CAN ID for CAN communication.
	configLocation->CANIDStyle                                     	= CANIDStyleVESC;          		// CAN ID default Style.
	configLocation->emitStatusOverCAN                              	= false;                   		// Send status over can.
	configLocation->canBusSpeed                                    	= canSpeedBaud500k;        		// 500k CAN baud
	configLocation->emitStatusProtocol                             	= canEmitProtocolVESC; 			// Can emit protocol set to MG style for backwards compatibility
	configLocation->tempEnableMaskBMS                              	= 0x0001;				// Bitwise select what sensor to enable for the BMS (internal sensors).
	configLocation->tempEnableMaskBattery                          	= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
	configLocation->tempEnableMaskExpansion                     	= 0xFFFF;				// Bitwise select what sensor to enable for the expansion boards(external sensors).
  	configLocation->noOfTempSensorPerModule            		= 1;					// Number of temperature sensors monitored per LTC68XX
	configLocation->noOfExpansionBoard   				= 0;					// Number of expansion board
	configLocation->noOfTempSensorPerExpansionBoard          	= 0;					// Number of temperature sensors monitored per expansion board
	configLocation->LCUseDischarge                                 	= enabled;                 		// Enable or disable the solid state output
	configLocation->LCUsePrecharge                                 	= enabled;              		// Use precharge before enabling main output
	configLocation->allowChargingDuringDischarge                   	= true;                    		// Allow the battery to be charged in normal mode
	configLocation->allowForceOn                                   	= false;                   		// Allow the BMS to be forced ON by long actuation of the power button
	configLocation->pulseToggleButton                              	= true;                    		// Select either pulse or toggle power button
	configLocation->useCANSafetyInput                              	= false;                   		// Use the safety input status from CAN
	configLocation->useCANDelayedPowerDown                         	= false;                   		// Use delayed power down
	configLocation->NTCTopResistor[modConfigNTCGroupLTCExt]        	= 100000;                  		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupMasterPCB]     	= 100000;                  		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupExp]  		= 100000;                  		// NTC Pullup resistor value
	configLocation->NTC25DegResistance[modConfigNTCGroupLTCExt]    	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupMasterPCB] 	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupExp] 	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTCBetaFactor[modConfigNTCGroupLTCExt]         	= 4250;                  		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupMasterPCB]      	= 4250;                    		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupExp]      	= 4250;                    		// NTC Beta factor
	configLocation->cellMonitorType                                	= CELL_MON_LTC6811_1;      		// Use the new cell voltage monitor
	configLocation->cellMonitorICCount                             	= 1;                       		// Only one slave IC
	configLocation->externalEnableOperationalState                 	= opStateExtNormal;        		// Go to normal enable mode
	configLocation->chargeEnableOperationalState                   	= opStateChargingModeCharging;		// Go to charging mode when a charger is connected
	configLocation->powerDownDelay                                 	= 3000;                    		// Wait only minimal to turn off

	configLocation->noOfCellsPerModule                             	= 12;                      		// Number of cell levels monitored per LTC68XX
	configLocation->lastICNoOfCells					= 0;
	configLocation->lastICMask					= 0;
	configLocation->humidityICType 					= 0;
	configLocation->BMSApplication					= electricVehicle;

#elif ENNOID_HV
	configLocation->noOfCellsSeries					= 12;					// Total number of cells in series in the battery pack
	configLocation->noOfCellsParallel                              	= 10;                      		// Number of cells in parallel
	configLocation->noOfParallelModules                       	= 1;                     		// Number of parallel modules
	configLocation->batteryCapacity					= 22.00f;				// XXAh battery
	configLocation->cellHardUnderVoltage				= 2.30f;				// Worst case X.XXV as lowest cell voltage
	configLocation->cellHardOverVoltage				= 4.20f;				// Worst case X.XXV as highest cell voltage
	configLocation->cellLCSoftUnderVoltage				= 2.70f;				// Lowest cell voltage X.XXV.
	configLocation->cellSoftOverVoltage				= 4.15f;				// Normal highest cell voltage X.XXV.
	configLocation->cellBalanceDifferenceThreshold                 	= 0.01f;				// Start balancing @ XmV difference, stop if below.
	configLocation->cellBalanceStart				= 4.1f;					// Start balancing above X.XXV.
	configLocation->cellBalanceAllTime				= false;				// Enable balancing under all opstate
	configLocation->cellThrottleUpperStart				= 0.03f;				// Upper range of cell voltage for charge throttling.
	configLocation->cellThrottleLowerStart				= 0.20f;				// Lower range of cell voltage for discharge throttling.
	configLocation->cellThrottleUpperMargin				= 0.01f;				// Margin of throttle from upper soft limits.
	configLocation->cellThrottleLowerMargin				= 0.50f;				// Margin of throttle from lower soft limits.	
	configLocation->packVoltageDataSource                          	= sourcePackVoltageISL28022; 		// Packvoltage source.
	configLocation->packCurrentDataSource                          	= sourcePackCurrentISL28022; 		// The pack current is the same as the current through the low current shunt
	configLocation->buzzerSignalSource                             	= buzzerSourceOn;         		// Stores what source shoud be taken to trigger
	configLocation->buzzerSignalPersistant                         	= false;                    		// Stores whether the buzzer should stay on after triggering
  	configLocation->shuntLCFactor                                  	= -0.07f;              			// Shunt factor low current
	configLocation->voltageLCFactor	                               	= 47.5f;                   		// Pack voltage factor 
	configLocation->voltageLCOffset                                	= 0;                       		// Pack voltage offset 
	configLocation->loadVoltageFactor				= 190.0f;                  		// Load voltage factor 
	configLocation->loadVoltageOffset                              	= 9.0f;                    		// Load voltage offset
	configLocation->chargerVoltageFactor	                        = 49.2f;                   		// Charger voltage factor 
	configLocation->chargerVoltageOffset                           	= 0.0f;                    		// Charger voltage offset
	configLocation->throttleChargeIncreaseRate                     	= 1;                       		// Percentage charge throttle increase rate per 100ms (cell voltage loop time)  
	configLocation->throttleDisChargeIncreaseRate                  	= 2;                       		// Percentage discharge throttle increase rate per 100ms (cell voltage loop time)  	
	configLocation->cellBalanceUpdateInterval			= 4*1000;				// Keep calculated resistors enabled for this amount of time in miliseconds.
	configLocation->maxSimultaneousDischargingCells			= 5;					// Allow a maximum of X cells simultinous discharging trough bleeding resistors.
	configLocation->timeoutDischargeRetry				= 10*1000;				// Wait for X seconds before retrying to enable load.
	configLocation->hysteresisDischarge 				= 0.02f;				// Lowest cell should rise XXmV before output is re enabled.
	configLocation->timeoutChargeRetry				= 30*1000;				// Wait for XX seconds before retrying to enable charger.
	configLocation->hysteresisCharge				= 0.01f;				// Highest cell should lower XXmV before charger is re enabled.
	configLocation->timeoutChargeCompleted				= 30*60*1000;				// Wait for XX minutes before setting charge state to charged.
	configLocation->timeoutChargingCompletedMinimalMismatch 	= 6*1000;				// If cell mismatch is under threshold and (charging is not allowed) wait this delay time to set "charged" state.
	configLocation->maxMismatchThreshold				= 0.010f;				// If mismatch is under this threshold for timeoutChargingCompletedMinimalMismatch determin fully charged.
	configLocation->chargerEnabledThreshold				= 0.5f;					// If charge current > X.XA stay in charging mode and dont power off.
	configLocation->timeoutChargerDisconnected			= 2000;					// Wait for X seconds to respond to charger disconnect.
	configLocation->minimalPrechargePercentage			= 0.70f;				// output should be at a minimal of 80% of input voltage.
	configLocation->timeoutLCPreCharge				= 1.5*1000;				// Precharge error timeout, allow 1.5 seconds pre-charge time before declaring load error.
	configLocation->maxAllowedCurrent				= 1000.0f;				// Allow max XXXA trough BMS.
	configLocation->allowedTempBattDischargingMax                  	= 75.0f;                  		// Max battery temperature where discharging is still allowed
	configLocation->allowedTempBattDischargingMin                  	= 0.0f;                    		// Min battery temperature where discharging is still allowed
	configLocation->allowedTempBattChargingMax                     	= 50.0f;                   		// Max battery temperature where charging is still allowed
	configLocation->allowedTempBattChargingMin                     	= 0.0f;                    		// Min battery temperature where charging is still allowed
	configLocation->allowedTempBattCoolingMax                      	= 5.0f;                    		// Max battery temperature where cooling is activated
	configLocation->allowedTempBattCoolingMin                      	= 50.0f;                   		// Min battery temperature where heating is activated
	configLocation->allowedTempBMSMax                              	= 80.0f;                   		// Max BMS operational temperature
	configLocation->allowedTempBMSMin                              	= 0.0f;                    		// Min BMS operational temperature
	configLocation->displayTimeoutBatteryDead			= 5000;					// Show battery dead symbol X seconds before going to powerdown in cell voltage error state.
	configLocation->displayTimeoutBatteryError			= 5000;					// Show error symbol for X seconds before going to powerdown in general error state.
	configLocation->displayTimeoutBatteryErrorPreCharge		= 10000;				// Show pre charge error for XX seconds.
	configLocation->displayTimeoutSplashScreen			= 3000;					// Display / INIT splash screen time.
	configLocation->displayStyle					= advanced;				// Display style used for showing the SSD1306 data
	configLocation->maxUnderAndOverVoltageErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->maxUnderAndOverTemperatureErrorCount		= 5;					// Max count of hard cell voltage errors.
	configLocation->notUsedCurrentThreshold				= 1.0f;					// If abs(packcurrent) < X.XA consider pack as not used.
	configLocation->notUsedTimeout					= 20*60*1000;				// If pack is not used for longer than XX minutes disable bms.
	configLocation->stateOfChargeStoreInterval			= 60*1000;				// Interval in ms to store state of charge information.
	configLocation->stateOfChargeMethod                            	= socCoulomb;				// Use coulomb counting for SoC calculation
	configLocation->CANID						= 10;					// CAN ID for CAN communication.
	configLocation->CANIDStyle                                     	= CANIDStyleVESC;          		// CAN ID default Style.
	configLocation->canBusSpeed                                    	= canSpeedBaud500k;        		// 500k CAN baud
	configLocation->emitStatusOverCAN                              	= false;                   		// Send status over can.
	configLocation->emitStatusProtocol                             	= canEmitProtocolVESC; 			// Can emit protocol set to MG style for backwards compatibility
	configLocation->tempEnableMaskBMS                              	= 0x0001;				// Bitwise select what sensor to enable for the BMS (internal sensors).
	configLocation->tempEnableMaskBattery                          	= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
  configLocation->tempEnableMaskExpansion                         	= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
	configLocation->noOfTempSensorPerModule            		= 1;					// Number of temperature sensors monitored per LTC68XX
	configLocation->noOfExpansionBoard       			= 0;					// Number of expansion board
	configLocation->noOfTempSensorPerExpansionBoard          	= 0;					// Number of temperature sensors monitored per expansion board
	configLocation->LCUseDischarge                                 	= enabled;                 		// Enable or disable the solid state output
	configLocation->LCUsePrecharge                                 	= enabled;              		// Use precharge before enabling main output
	configLocation->allowChargingDuringDischarge                   	= true;                    		// Allow the battery to be charged in normal mode
	configLocation->allowForceOn                                   	= false;                  		// Allow the BMS to be forced ON by long actuation of the power button
	configLocation->pulseToggleButton                              	= true;                   		// Select either pulse or toggle power button
	configLocation->useCANSafetyInput                              	= false;             			// Use the safety input status from CAN
	configLocation->useCANDelayedPowerDown                         	= false;                  		// Use delayed power down
	configLocation->NTCTopResistor[modConfigNTCGroupLTCExt]        	= 100000;                 		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupMasterPCB]     	= 100000;                 		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupExp]    	= 100000;                 		// NTC Pullup resistor value
	configLocation->NTC25DegResistance[modConfigNTCGroupLTCExt]    	= 100000;                 		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupMasterPCB] 	= 100000;                 		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupExp] 	= 100000;                 		// NTC resistance at 25 degree
	configLocation->NTCBetaFactor[modConfigNTCGroupLTCExt]         	= 4250;                   		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupMasterPCB]      	= 4250;                   		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupExp]		= 4250;                  		// NTC Beta factor
	configLocation->cellMonitorType                                	= CELL_MON_LTC6811_1;     		// Use the new cell voltage monitor
	configLocation->cellMonitorICCount                             	= 1;                      		// Only one slave IC
	configLocation->externalEnableOperationalState                 	= opStateExtNormal;       		// Go to normal enable mode
	configLocation->chargeEnableOperationalState                   	= opStateChargingModeCharging;		// Go to charging mode when a charger is connected
	configLocation->powerDownDelay                                 	= 3000;                   		// Wait only minimal to turn off
	
	configLocation->noOfCellsPerModule                             	= 12;                      		// Number of cell levels monitored per LTC68XX	
	configLocation->lastICNoOfCells					= 0;
	configLocation->lastICMask					= 0;
	configLocation->humidityICType 					= 0;
	configLocation->cellTypeUsed					= 0;
	configLocation->firstICNoCells					= 0;
	configLocation->BMSApplication					= electricVehicle;

#elif ENNOID_HV_2
	configLocation->noOfCellsSeries					= 12;					// Total number of cells in series in the battery pack
	configLocation->noOfCellsParallel                              	= 10;                      		// Number of cells in parallel
	configLocation->noOfParallelModules                       	= 1;                     		// Number of parallel modules
	configLocation->batteryCapacity					= 22.00f;				// XXAh battery
	configLocation->cellHardUnderVoltage				= 2.30f;				// Worst case X.XXV as lowest cell voltage
	configLocation->cellHardOverVoltage				= 4.20f;				// Worst case X.XXV as highest cell voltage
	configLocation->cellLCSoftUnderVoltage				= 2.70f;				// Lowest cell voltage X.XXV.
	configLocation->cellSoftOverVoltage				= 4.15f;				// Normal highest cell voltage X.XXV.
	configLocation->cellBalanceDifferenceThreshold                 	= 0.01f;				// Start balancing @ XmV difference, stop if below.
	configLocation->cellBalanceStart				= 4.1f;					// Start balancing above X.XXV.
	configLocation->cellBalanceAllTime				= false;				// Enable balancing under all opstate
	configLocation->cellThrottleUpperStart				= 0.03f;				// Upper range of cell voltage for charge throttling.
	configLocation->cellThrottleLowerStart				= 0.20f;				// Lower range of cell voltage for discharge throttling.
	configLocation->cellThrottleUpperMargin				= 0.01f;				// Margin of throttle from upper soft limits.
	configLocation->cellThrottleLowerMargin				= 0.50f;				// Margin of throttle from lower soft limits.	
	configLocation->packVoltageDataSource                          	= sourcePackVoltageISL28022; 		// Packvoltage source.
	configLocation->packCurrentDataSource                          	= sourcePackCurrentISL28022; 		// The pack current is the same as the current through the low current shunt
	configLocation->buzzerSignalSource                             	= buzzerSourceOn;         		// Stores what source shoud be taken to trigger
	configLocation->buzzerSignalPersistant                         	= false;                    		// Stores whether the buzzer should stay on after triggering
  	configLocation->shuntLCFactor                                  	= -0.07f;              			// Shunt factor low current
	configLocation->voltageLCFactor	                               	= 47.5f;                   		// Pack voltage factor 
	configLocation->voltageLCOffset                                	= 0;                       		// Pack voltage offset 
	configLocation->loadVoltageFactor				= 190.0f;                  		// Load voltage factor 
	configLocation->loadVoltageOffset                              	= 9.0f;                    		// Load voltage offset
	configLocation->chargerVoltageFactor	                        = 49.2f;                   		// Charger voltage factor 
	configLocation->chargerVoltageOffset                           	= 0.0f;                    		// Charger voltage offset
	configLocation->throttleChargeIncreaseRate                     	= 1;                       		// Percentage charge throttle increase rate per 100ms (cell voltage loop time)  
	configLocation->throttleDisChargeIncreaseRate                  	= 2;                       		// Percentage discharge throttle increase rate per 100ms (cell voltage loop time)  	
	configLocation->cellBalanceUpdateInterval			= 4*1000;				// Keep calculated resistors enabled for this amount of time in miliseconds.
	configLocation->maxSimultaneousDischargingCells			= 5;					// Allow a maximum of X cells simultinous discharging trough bleeding resistors.
	configLocation->timeoutDischargeRetry				= 10*1000;				// Wait for X seconds before retrying to enable load.
	configLocation->hysteresisDischarge 				= 0.02f;				// Lowest cell should rise XXmV before output is re enabled.
	configLocation->timeoutChargeRetry				= 30*1000;				// Wait for XX seconds before retrying to enable charger.
	configLocation->hysteresisCharge				= 0.01f;				// Highest cell should lower XXmV before charger is re enabled.
	configLocation->timeoutChargeCompleted				= 30*60*1000;				// Wait for XX minutes before setting charge state to charged.
	configLocation->timeoutChargingCompletedMinimalMismatch 	= 6*1000;				// If cell mismatch is under threshold and (charging is not allowed) wait this delay time to set "charged" state.
	configLocation->maxMismatchThreshold				= 0.010f;				// If mismatch is under this threshold for timeoutChargingCompletedMinimalMismatch determin fully charged.
	configLocation->chargerEnabledThreshold				= 0.5f;					// If charge current > X.XA stay in charging mode and dont power off.
	configLocation->timeoutChargerDisconnected			= 2000;					// Wait for X seconds to respond to charger disconnect.
	configLocation->minimalPrechargePercentage			= 0.70f;				// output should be at a minimal of 80% of input voltage.
	configLocation->timeoutLCPreCharge				= 1.5*1000;				// Precharge error timeout, allow 1.5 seconds pre-charge time before declaring load error.
	configLocation->maxAllowedCurrent				= 1000.0f;				// Allow max XXXA trough BMS.
	configLocation->allowedTempBattDischargingMax                  	= 75.0f;                  		// Max battery temperature where discharging is still allowed
	configLocation->allowedTempBattDischargingMin                  	= 0.0f;                    		// Min battery temperature where discharging is still allowed
	configLocation->allowedTempBattChargingMax                     	= 50.0f;                   		// Max battery temperature where charging is still allowed
	configLocation->allowedTempBattChargingMin                     	= 0.0f;                    		// Min battery temperature where charging is still allowed
	configLocation->allowedTempBattCoolingMax                      	= 5.0f;                    		// Max battery temperature where cooling is activated
	configLocation->allowedTempBattCoolingMin                      	= 50.0f;                   		// Min battery temperature where heating is activated
	configLocation->allowedTempBMSMax                              	= 80.0f;                   		// Max BMS operational temperature
	configLocation->allowedTempBMSMin                              	= 0.0f;                    		// Min BMS operational temperature
	configLocation->displayTimeoutBatteryDead			= 5000;					// Show battery dead symbol X seconds before going to powerdown in cell voltage error state.
	configLocation->displayTimeoutBatteryError			= 5000;					// Show error symbol for X seconds before going to powerdown in general error state.
	configLocation->displayTimeoutBatteryErrorPreCharge		= 10000;				// Show pre charge error for XX seconds.
	configLocation->displayTimeoutSplashScreen			= 3000;					// Display / INIT splash screen time.
	configLocation->displayStyle					= advanced;				// Display style used for showing the SSD1306 data
	configLocation->maxUnderAndOverVoltageErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->maxUnderAndOverTemperatureErrorCount		= 5;					// Max count of hard cell voltage errors.
	configLocation->notUsedCurrentThreshold				= 1.0f;					// If abs(packcurrent) < X.XA consider pack as not used.
	configLocation->notUsedTimeout					= 20*60*1000;				// If pack is not used for longer than XX minutes disable bms.
	configLocation->stateOfChargeStoreInterval			= 60*1000;				// Interval in ms to store state of charge information.
	configLocation->stateOfChargeMethod                            	= socCoulomb;				// Use coulomb counting for SoC calculation
	configLocation->CANID						= 10;					// CAN ID for CAN communication.
	configLocation->CANIDStyle                                     	= CANIDStyleVESC;          		// CAN ID default Style.
	configLocation->canBusSpeed                                    	= canSpeedBaud500k;        		// 500k CAN baud
	configLocation->emitStatusOverCAN                              	= false;                   		// Send status over can.
	configLocation->emitStatusProtocol                             	= canEmitProtocolVESC; 			// Can emit protocol set to MG style for backwards compatibility
	configLocation->tempEnableMaskBMS                              	= 0x0001;				// Bitwise select what sensor to enable for the BMS (internal sensors).
	configLocation->tempEnableMaskBattery                          	= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
  	configLocation->tempEnableMaskExpansion                         = 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
	configLocation->noOfTempSensorPerModule            		= 1;					// Number of temperature sensors monitored per LTC68XX
	configLocation->noOfExpansionBoard       			= 0;					// Number of expansion board
	configLocation->noOfTempSensorPerExpansionBoard          	= 0;					// Number of temperature sensors monitored per expansion board
	configLocation->LCUseDischarge                                 	= enabled;                 		// Enable or disable the solid state output
	configLocation->LCUsePrecharge                                 	= enabled;              		// Use precharge before enabling main output
	configLocation->allowChargingDuringDischarge                   	= true;                    		// Allow the battery to be charged in normal mode
	configLocation->allowForceOn                                   	= false;                  		// Allow the BMS to be forced ON by long actuation of the power button
	configLocation->pulseToggleButton                              	= true;                   		// Select either pulse or toggle power button
	configLocation->useCANSafetyInput                              	= false;             			// Use the safety input status from CAN
	configLocation->useCANDelayedPowerDown                         	= false;                  		// Use delayed power down
	configLocation->NTCTopResistor[modConfigNTCGroupLTCExt]        	= 100000;                 		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupMasterPCB]     	= 100000;                 		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupExp]    	= 100000;                 		// NTC Pullup resistor value
	configLocation->NTC25DegResistance[modConfigNTCGroupLTCExt]    	= 100000;                 		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupMasterPCB] 	= 100000;                 		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupExp] 	= 100000;                 		// NTC resistance at 25 degree
	configLocation->NTCBetaFactor[modConfigNTCGroupLTCExt]         	= 4250;                   		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupMasterPCB]      	= 4250;                   		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupExp]		= 4250;                  		// NTC Beta factor
	configLocation->cellMonitorType                                	= CELL_MON_LTC6811_1;     		// Use the new cell voltage monitor
	configLocation->cellMonitorICCount                             	= 1;                      		// Only one slave IC
	configLocation->externalEnableOperationalState                 	= opStateExtNormal;       		// Go to normal enable mode
	configLocation->chargeEnableOperationalState                   	= opStateChargingModeCharging;		// Go to charging mode when a charger is connected
	configLocation->powerDownDelay                                 	= 3000;                   		// Wait only minimal to turn off
	
	configLocation->noOfCellsPerModule                             	= 12;                      		// Number of cell levels monitored per LTC68XX	
	configLocation->lastICNoOfCells					= 0;
	configLocation->lastICMask					= 0;
	configLocation->humidityICType 					= 2;
	configLocation->cellTypeUsed					= 0;
	configLocation->firstICNoCells					= 0;
	configLocation->BMSApplication					= electricVehicle;

#elif ENNOID_SS
	configLocation->noOfCellsSeries					= 18;					// Total number of cells in series in the battery pack
	configLocation->noOfCellsParallel                              	= 10;                      		// Number of cells in parallel
	configLocation->noOfParallelModules                       	= 1;                     		// Number of parallel modules
	configLocation->batteryCapacity					= 22.00f;				// XXAh battery
	configLocation->cellHardUnderVoltage				= 2.30f;				// Worst case X.XXV as lowest cell voltage
	configLocation->cellHardOverVoltage				= 4.20f;				// Worst case X.XXV as highest cell voltage
	configLocation->cellLCSoftUnderVoltage				= 2.70f;				// Lowest cell voltage X.XXV.
	configLocation->cellSoftOverVoltage				= 4.15f;				// Normal highest cell voltage X.XXV.
	configLocation->cellBalanceDifferenceThreshold                 	= 0.01f;				// Start balancing @ XmV difference, stop if below.
	configLocation->cellBalanceStart				= 4.1f;					// Start balancing above X.XXV.
	configLocation->cellBalanceAllTime				= false;				// Enable balancing under all opstate
	configLocation->cellThrottleUpperStart				= 0.03f;				// Upper range of cell voltage for charge throttling.
	configLocation->cellThrottleLowerStart				= 0.20f;				// Lower range of cell voltage for discharge throttling.
	configLocation->cellThrottleUpperMargin				= 0.01f;				// Margin of throttle from upper soft limits.
	configLocation->cellThrottleLowerMargin				= 0.50f;				// Margin of throttle from lower soft limits.	
	configLocation->packVoltageDataSource                          	= sourcePackVoltageISL28022;		// Packvoltage source.
	configLocation->packCurrentDataSource                          	= sourcePackCurrentISL28022; 		// The pack current is the same as the current through the low current shunt
	configLocation->buzzerSignalSource                             	= buzzerSourceOn;         		// Stores what source shoud be taken to trigger
	configLocation->buzzerSignalPersistant                         	= false;                    		// Stores whether the buzzer should stay on after triggering
  	configLocation->shuntLCFactor                                  	= -0.0052f;              		// Shunt factor low current
	configLocation->voltageLCFactor	                               	= 1.35f;                   		// Pack voltage factor 
	configLocation->voltageLCOffset                                	= 0;                       		// Pack voltage offset 
	configLocation->loadVoltageFactor	                        = 29.8f;   		                // Load voltage factor 
	configLocation->loadVoltageOffset                              	= 0.0f;                   		// Load voltage offset
	configLocation->chargerVoltageFactor	                        = 29.8f;      			        // Charger voltage factor 
	configLocation->chargerVoltageOffset                           	= 0.0f;                    		// Charger voltage offset
	configLocation->throttleChargeIncreaseRate                     	= 1;                       		// Percentage charge throttle increase rate per 100ms (cell voltage loop time)  
	configLocation->throttleDisChargeIncreaseRate                  	= 2;                      		// Percentage discharge throttle increase rate per 100ms (cell voltage loop time)  	
	configLocation->cellBalanceUpdateInterval			= 4*1000;				// Keep calculated resistors enabled for this amount of time in miliseconds.
	configLocation->maxSimultaneousDischargingCells			= 5;					// Allow a maximum of X cells simultinous discharging trough bleeding resistors.
	configLocation->timeoutDischargeRetry				= 10*1000;				// Wait for X seconds before retrying to enable load.
	configLocation->hysteresisDischarge 				= 0.02f;				// Lowest cell should rise XXmV before output is re enabled.
	configLocation->timeoutChargeRetry				= 30*1000;				// Wait for XX seconds before retrying to enable charger.
	configLocation->hysteresisCharge				= 0.01f;				// Highest cell should lower XXmV before charger is re enabled.
	configLocation->timeoutChargeCompleted				= 30*60*1000;				// Wait for XX minutes before setting charge state to charged.
	configLocation->timeoutChargingCompletedMinimalMismatch 	= 6*1000;				// If cell mismatch is under threshold and (charging is not allowed) wait this delay time to set "charged" state.
	configLocation->maxMismatchThreshold				= 0.010f;				// If mismatch is under this threshold for timeoutChargingCompletedMinimalMismatch determin fully charged.
	configLocation->chargerEnabledThreshold				= 0.5f;					// If charge current > X.XA stay in charging mode and dont power off.
	configLocation->timeoutChargerDisconnected			= 2000;					// Wait for X seconds to respond to charger disconnect.
	configLocation->minimalPrechargePercentage			= 0.70f;				// output should be at a minimal of 80% of input voltage.
	configLocation->timeoutLCPreCharge				= 1.5*1000;				// Precharge error timeout, allow 1.5 seconds pre-charge time before declaring load error.
	configLocation->maxAllowedCurrent				= 1000.0f;				// Allow max XXXA trough BMS.
	configLocation->allowedTempBattDischargingMax                  	= 105.0f;                   		// Max battery temperature where discharging is still allowed
	configLocation->allowedTempBattDischargingMin                  	= 0.0f;                    		// Min battery temperature where discharging is still allowed
	configLocation->allowedTempBattChargingMax                     	= 105.0f;                   		// Max battery temperature where charging is still allowed
	configLocation->allowedTempBattChargingMin                     	= 0.0f;                    		// Min battery temperature where charging is still allowed
	configLocation->allowedTempBattCoolingMax                      	= 5.0f;                    		// Max battery temperature where cooling is activated
	configLocation->allowedTempBattCoolingMin                      	= 50.0f;                   		// Min battery temperature where heating is activated
	configLocation->allowedTempBMSMax                              	= 80.0f;                   		// Max BMS operational temperature
	configLocation->allowedTempBMSMin                              	= 0.0f;                    		// Min BMS operational temperature
	configLocation->displayTimeoutBatteryDead			= 5000;					// Show battery dead symbol X seconds before going to powerdown in cell voltage error state.
	configLocation->displayTimeoutBatteryError			= 5000;					// Show error symbol for X seconds before going to powerdown in general error state.
	configLocation->displayTimeoutBatteryErrorPreCharge		= 10000;				// Show pre charge error for XX seconds.
	configLocation->displayTimeoutSplashScreen			= 3000;					// Display / INIT splash screen time.
	configLocation->displayStyle					= advanced;				// Display style used for showing the SSD1306 data
	configLocation->maxUnderAndOverVoltageErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->maxUnderAndOverTemperatureErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->notUsedCurrentThreshold				= 1.0f;					// If abs(packcurrent) < X.XA consider pack as not used.
	configLocation->notUsedTimeout					= 20*60*1000;				// If pack is not used for longer than XX minutes disable bms.
	configLocation->stateOfChargeStoreInterval			= 60*1000;				// Interval in ms to store state of charge information.
	configLocation->stateOfChargeMethod                            	= socCoulomb;              		// Use coulomb counting for SoC calculation
	configLocation->CANID						= 10;					// CAN ID for CAN communication.
	configLocation->CANIDStyle                                     	= CANIDStyleVESC;		        // CAN ID default Style.
	configLocation->canBusSpeed                                    	= canSpeedBaud500k;        		// 500k CAN baud
	configLocation->emitStatusOverCAN                              	= false;                   		// Send status over can.
	configLocation->emitStatusProtocol                             	= canEmitProtocolVESC; 			// Can emit protocol set to MG style for backwards compatibility
	configLocation->tempEnableMaskBMS                              	= 0x0001;				// Bitwise select what sensor to enable for the BMS (internal sensors).
	configLocation->tempEnableMaskBattery                          	= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
  	configLocation->tempEnableMaskExpansion                         = 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
	configLocation->noOfTempSensorPerModule            		= 0;					// Number of temperature sensors monitored per LTC68XX
	configLocation->noOfExpansionBoard       			= 0;					// Number of expansion board
	configLocation->noOfTempSensorPerExpansionBoard          	= 0;					// Number of temperature sensors monitored per expansion board
	configLocation->LCUseDischarge                                 	= enabled;                 		// Enable or disable the solid state output
	configLocation->LCUsePrecharge                                 	= enabled;              		// Use precharge before enabling main output
	configLocation->allowChargingDuringDischarge                   	= false;                    		// Allow the battery to be charged in normal mode
	configLocation->allowForceOn                                   	= false;                   		// Allow the BMS to be forced ON by long actuation of the power button
	configLocation->pulseToggleButton                             	= true;                    		// Select either pulse or toggle power button
	configLocation->useCANSafetyInput                              	= false;                   		// Use the safety input status from CAN
	configLocation->useCANDelayedPowerDown                         	= false;                   		// Use delayed power down
	configLocation->NTCTopResistor[modConfigNTCGroupLTCExt]        	= 100000;                 		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupMasterPCB]     	= 100000;                  		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupExp]    	= 100000;                  		// NTC Pullup resistor value
	configLocation->NTC25DegResistance[modConfigNTCGroupLTCExt]    	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupMasterPCB] 	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupExp] 	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTCBetaFactor[modConfigNTCGroupLTCExt]         	= 4250;                    		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupMasterPCB]      	= 4250;                    		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupExp]		= 4250;                    		// NTC Beta factor
	configLocation->cellMonitorType                               	= CELL_MON_LTC6813_1;      		// Use the new cell voltage monitor
	configLocation->cellMonitorICCount                             	= 1;                       		// Only one slave IC
	configLocation->externalEnableOperationalState                 	= opStateExtNormal;        		// Go to normal enable mode
	configLocation->chargeEnableOperationalState                   	= opStateChargingModeCharging;		// Go to charging mode when a charger is connected
	configLocation->powerDownDelay                                 	= 3000;                    		// Wait only minimal to turn off
	configLocation->noOfCellsPerModule                             	= 18;                      		// Number of cell levels monitored per LTC68XX	
	configLocation->lastICNoOfCells					= 0;					// Last IC cell count (calculated)
	configLocation->lastICMask					= 0;					// Balance mask used for last IC
	configLocation->humidityICType 					= 2;					// Humidity sensor IC type
	configLocation->BMSApplication					= electricVehicle;

#elif ENNOID_SS_24
	configLocation->noOfCellsSeries					= 24;					// Total number of cells in series in the battery pack
	configLocation->noOfCellsParallel                              	= 10;                      		// Number of cells in parallel
	configLocation->noOfParallelModules                       	= 1;                     		// Number of parallel modules
	configLocation->batteryCapacity					= 22.00f;				// XXAh battery
	configLocation->cellHardUnderVoltage				= 2.30f;				// Worst case X.XXV as lowest cell voltage
	configLocation->cellHardOverVoltage				= 4.20f;				// Worst case X.XXV as highest cell voltage
	configLocation->cellLCSoftUnderVoltage				= 2.70f;				// Lowest cell voltage X.XXV.
	configLocation->cellSoftOverVoltage				= 4.15f;				// Normal highest cell voltage X.XXV.
	configLocation->cellBalanceDifferenceThreshold                 	= 0.01f;				// Start balancing @ XmV difference, stop if below.
	configLocation->cellBalanceStart				= 4.1f;					// Start balancing above X.XXV.
	configLocation->cellBalanceAllTime				= false;				// Enable balancing under all opstate
	configLocation->cellThrottleUpperStart				= 0.03f;				// Upper range of cell voltage for charge throttling.
	configLocation->cellThrottleLowerStart				= 0.20f;				// Lower range of cell voltage for discharge throttling.
	configLocation->cellThrottleUpperMargin				= 0.01f;				// Margin of throttle from upper soft limits.
	configLocation->cellThrottleLowerMargin				= 0.50f;				// Margin of throttle from lower soft limits.	
	configLocation->packVoltageDataSource                          	= sourcePackVoltageISL28022;		// Packvoltage source.
	configLocation->packCurrentDataSource                          	= sourcePackCurrentISL28022; 		// The pack current is the same as the current through the low current shunt
	configLocation->buzzerSignalSource                             	= buzzerSourceOn;         		// Stores what source shoud be taken to trigger
	configLocation->buzzerSignalPersistant                         	= false;                    		// Stores whether the buzzer should stay on after triggering
  	configLocation->shuntLCFactor                                  	= -0.0104f;              		// Shunt factor low current
	configLocation->voltageLCFactor	                               	= 2.55f;                   		// Pack voltage factor 
	configLocation->voltageLCOffset                                	= 0;                       		// Pack voltage offset 
	configLocation->loadVoltageFactor	                        = 39.0f;   		                // Load voltage factor 
	configLocation->loadVoltageOffset                              	= 0.0f;                   		// Load voltage offset
	configLocation->chargerVoltageFactor	                        = 39.0f;      			        // Charger voltage factor 
	configLocation->chargerVoltageOffset                           	= 0.0f;                    		// Charger voltage offset
	configLocation->throttleChargeIncreaseRate                     	= 1;                       		// Percentage charge throttle increase rate per 100ms (cell voltage loop time)  
	configLocation->throttleDisChargeIncreaseRate                  	= 2;                      		// Percentage discharge throttle increase rate per 100ms (cell voltage loop time)  	
	configLocation->cellBalanceUpdateInterval			= 4*1000;				// Keep calculated resistors enabled for this amount of time in miliseconds.
	configLocation->maxSimultaneousDischargingCells			= 5;					// Allow a maximum of X cells simultinous discharging trough bleeding resistors.
	configLocation->timeoutDischargeRetry				= 10*1000;				// Wait for X seconds before retrying to enable load.
	configLocation->hysteresisDischarge 				= 0.02f;				// Lowest cell should rise XXmV before output is re enabled.
	configLocation->timeoutChargeRetry				= 30*1000;				// Wait for XX seconds before retrying to enable charger.
	configLocation->hysteresisCharge				= 0.01f;				// Highest cell should lower XXmV before charger is re enabled.
	configLocation->timeoutChargeCompleted				= 30*60*1000;				// Wait for XX minutes before setting charge state to charged.
	configLocation->timeoutChargingCompletedMinimalMismatch 	= 6*1000;				// If cell mismatch is under threshold and (charging is not allowed) wait this delay time to set "charged" state.
	configLocation->maxMismatchThreshold				= 0.010f;				// If mismatch is under this threshold for timeoutChargingCompletedMinimalMismatch determin fully charged.
	configLocation->chargerEnabledThreshold				= 0.5f;					// If charge current > X.XA stay in charging mode and dont power off.
	configLocation->timeoutChargerDisconnected			= 2000;					// Wait for X seconds to respond to charger disconnect.
	configLocation->minimalPrechargePercentage			= 0.70f;				// output should be at a minimal of 80% of input voltage.
	configLocation->timeoutLCPreCharge				= 1.5*1000;				// Precharge error timeout, allow 1.5 seconds pre-charge time before declaring load error.
	configLocation->maxAllowedCurrent				= 1000.0f;				// Allow max XXXA trough BMS.
	configLocation->allowedTempBattDischargingMax                  	= 105.0f;                   		// Max battery temperature where discharging is still allowed
	configLocation->allowedTempBattDischargingMin                  	= 0.0f;                    		// Min battery temperature where discharging is still allowed
	configLocation->allowedTempBattChargingMax                     	= 105.0f;                   		// Max battery temperature where charging is still allowed
	configLocation->allowedTempBattChargingMin                     	= 0.0f;                    		// Min battery temperature where charging is still allowed
	configLocation->allowedTempBattCoolingMax                      	= 5.0f;                    		// Max battery temperature where cooling is activated
	configLocation->allowedTempBattCoolingMin                      	= 50.0f;                   		// Min battery temperature where heating is activated
	configLocation->allowedTempBMSMax                              	= 80.0f;                   		// Max BMS operational temperature
	configLocation->allowedTempBMSMin                              	= 0.0f;                    		// Min BMS operational temperature
	configLocation->displayTimeoutBatteryDead			= 5000;					// Show battery dead symbol X seconds before going to powerdown in cell voltage error state.
	configLocation->displayTimeoutBatteryError			= 5000;					// Show error symbol for X seconds before going to powerdown in general error state.
	configLocation->displayTimeoutBatteryErrorPreCharge		= 10000;				// Show pre charge error for XX seconds.
	configLocation->displayTimeoutSplashScreen			= 3000;					// Display / INIT splash screen time.
	configLocation->displayStyle					= advanced;				// Display style used for showing the SSD1306 data
	configLocation->maxUnderAndOverVoltageErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->maxUnderAndOverTemperatureErrorCount 		= 5;					// Max count of hard cell voltage errors.
	configLocation->notUsedCurrentThreshold				= 1.0f;					// If abs(packcurrent) < X.XA consider pack as not used.
	configLocation->notUsedTimeout					= 20*60*1000;				// If pack is not used for longer than XX minutes disable bms.
	configLocation->stateOfChargeStoreInterval			= 60*1000;				// Interval in ms to store state of charge information.
	configLocation->stateOfChargeMethod                            	= socCoulomb;              		// Use coulomb counting for SoC calculation
	configLocation->CANID						= 10;					// CAN ID for CAN communication.
	configLocation->CANIDStyle                                     	= CANIDStyleVESC;		        // CAN ID default Style.
	configLocation->canBusSpeed                                    	= canSpeedBaud500k;        		// 500k CAN baud
	configLocation->emitStatusOverCAN                              	= false;                   		// Send status over can.
	configLocation->emitStatusProtocol                             	= canEmitProtocolVESC; 			// Can emit protocol set to MG style for backwards compatibility
	configLocation->tempEnableMaskBMS                              	= 0x0001;				// Bitwise select what sensor to enable for the BMS (internal sensors).
	configLocation->tempEnableMaskBattery                          	= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
  	configLocation->tempEnableMaskExpansion                         = 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
	configLocation->noOfTempSensorPerModule            		= 0;					// Number of temperature sensors monitored per LTC68XX
	configLocation->noOfExpansionBoard       			= 0;					// Number of expansion board
	configLocation->noOfTempSensorPerExpansionBoard          	= 0;					// Number of temperature sensors monitored per expansion board
	configLocation->LCUseDischarge                                 	= enabled;                 		// Enable or disable the solid state output
	configLocation->LCUsePrecharge                                 	= enabled;              		// Use precharge before enabling main output
	configLocation->allowChargingDuringDischarge                   	= false;                    		// Allow the battery to be charged in normal mode
	configLocation->allowForceOn                                   	= false;                   		// Allow the BMS to be forced ON by long actuation of the power button
	configLocation->pulseToggleButton                             	= true;                    		// Select either pulse or toggle power button
	configLocation->useCANSafetyInput                              	= false;                   		// Use the safety input status from CAN
	configLocation->useCANDelayedPowerDown                         	= false;                   		// Use delayed power down
	configLocation->NTCTopResistor[modConfigNTCGroupLTCExt]        	= 100000;                 		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupMasterPCB]     	= 100000;                  		// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupExp]    	= 100000;                  		// NTC Pullup resistor value
	configLocation->NTC25DegResistance[modConfigNTCGroupLTCExt]    	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupMasterPCB] 	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupExp] 	= 100000;                  		// NTC resistance at 25 degree
	configLocation->NTCBetaFactor[modConfigNTCGroupLTCExt]         	= 4250;                    		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupMasterPCB]      	= 4250;                    		// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupExp]		= 4250;                    		// NTC Beta factor
	configLocation->cellMonitorType                               	= CELL_MON_LTC6811_1;      		// Use the new cell voltage monitor
	configLocation->cellMonitorICCount                             	= 2;                       		// Number of monitoring IC 
	configLocation->externalEnableOperationalState                 	= opStateExtNormal;        		// Go to normal enable mode
	configLocation->chargeEnableOperationalState                   	= opStateChargingModeCharging;		// Go to charging mode when a charger is connected
	configLocation->powerDownDelay                                 	= 3000;                    		// Wait only minimal to turn off
	configLocation->noOfCellsPerModule                             	= 18;                      		// Number of cell levels monitored per LTC68XX	
	configLocation->lastICNoOfCells					= 0;					// Last IC cell count (calculated)
	configLocation->lastICMask					= 0;					// Balance mask used for last IC
	configLocation->humidityICType 					= 2;
	configLocation->BMSApplication					= electricVehicle;					// Humidity sensor IC type

#elif ENNOID_SS_LITE 
	configLocation->noOfCellsSeries					= 18;					// Total number of cells in series in the battery pack
	configLocation->noOfCellsParallel				= 10;					// Number of cells in parallel
	configLocation->noOfParallelModules				= 1;					// Number of parallel modules
	configLocation->batteryCapacity					= 22.00f;				// XXAh battery
	configLocation->cellHardUnderVoltage				= 2.30f;				// Worst case X.XXV as lowest cell voltage
	configLocation->cellHardOverVoltage				= 4.20f;				// Worst case X.XXV as highest cell voltage
	configLocation->cellLCSoftUnderVoltage				= 2.70f;				// Lowest cell voltage X.XXV.
	configLocation->cellSoftOverVoltage				= 4.15f;				// Normal highest cell voltage X.XXV.
	configLocation->cellBalanceDifferenceThreshold			= 0.01f;				// Start balancing @ XmV difference, stop if below.
	configLocation->cellBalanceStart				= 4.1f;					// Start balancing above X.XXV.
	configLocation->cellBalanceAllTime				= false;				// Enable balancing under all opstate
	configLocation->cellThrottleUpperStart				= 0.03f;				// Upper range of cell voltage for charge throttling.
	configLocation->cellThrottleLowerStart				= 0.20f;				// Lower range of cell voltage for discharge throttling.
	configLocation->cellThrottleUpperMargin				= 0.01f;				// Margin of throttle from upper soft limits.
	configLocation->cellThrottleLowerMargin				= 0.50f;				// Margin of throttle from lower soft limits.	
	configLocation->packVoltageDataSource				= sourcePackVoltageISL28022;		// Packvoltage source.
	configLocation->packCurrentDataSource				= sourcePackCurrentISL28022;		// The pack current is the same as the current through the low current shunt
	configLocation->buzzerSignalSource				= buzzerSourceOn;			// Stores what source shoud be taken to trigger
	configLocation->buzzerSignalPersistant				= false;				// Stores whether the buzzer should stay on after triggering
	configLocation->shuntLCFactor					= -0.0052f;              		// Shunt factor low current
	configLocation->voltageLCFactor					= 1.35f;				// Pack voltage factor 
	configLocation->voltageLCOffset					= 0;					// Pack voltage offset 
	configLocation->loadVoltageFactor				= 29.8f;				// Load voltage factor 
	configLocation->loadVoltageOffset				= 0.0f;					// Load voltage offset
	configLocation->chargerVoltageFactor				= 29.8f;				// Charger voltage factor 
	configLocation->chargerVoltageOffset				= 0.0f;					// Charger voltage offset
	configLocation->throttleChargeIncreaseRate			= 1;					// Percentage charge throttle increase rate per 100ms (cell voltage loop time)  
	configLocation->throttleDisChargeIncreaseRate			= 2;					// Percentage discharge throttle increase rate per 100ms (cell voltage loop time)  	
	configLocation->cellBalanceUpdateInterval			= 4*1000;				// Keep calculated resistors enabled for this amount of time in miliseconds.
	configLocation->maxSimultaneousDischargingCells			= 5;					// Allow a maximum of X cells simultinous discharging trough bleeding resistors.
	configLocation->timeoutDischargeRetry				= 10*1000;				// Wait for X seconds before retrying to enable load.
	configLocation->hysteresisDischarge				= 0.02f;				// Lowest cell should rise XXmV before output is re enabled.
	configLocation->timeoutChargeRetry				= 30*1000;				// Wait for XX seconds before retrying to enable charger.
	configLocation->hysteresisCharge				= 0.01f;				// Highest cell should lower XXmV before charger is re enabled.
	configLocation->timeoutChargeCompleted				= 30*60*1000;				// Wait for XX minutes before setting charge state to charged.
	configLocation->timeoutChargingCompletedMinimalMismatch		= 6*1000;				// If cell mismatch is under threshold and (charging is not allowed) wait this delay time to set "charged" state.
	configLocation->maxMismatchThreshold				= 0.010f;				// If mismatch is under this threshold for timeoutChargingCompletedMinimalMismatch determin fully charged.
	configLocation->chargerEnabledThreshold				= 0.5f;					// If charge current > X.XA stay in charging mode and dont power off.
	configLocation->timeoutChargerDisconnected			= 2000;					// Wait for X seconds to respond to charger disconnect.
	configLocation->minimalPrechargePercentage			= 0.70f;				// output should be at a minimal of 80% of input voltage.
	configLocation->timeoutLCPreCharge				= 1.5*1000;				// Precharge error timeout, allow 1.5 seconds pre-charge time before declaring load error.
	configLocation->maxAllowedCurrent				= 1000.0f;				// Allow max XXXA trough BMS.
	configLocation->allowedTempBattDischargingMax			= 105.0f;				// Max battery temperature where discharging is still allowed
	configLocation->allowedTempBattDischargingMin			= 0.0f;					// Min battery temperature where discharging is still allowed
	configLocation->allowedTempBattChargingMax			= 105.0f;				// Max battery temperature where charging is still allowed
	configLocation->allowedTempBattChargingMin			= 0.0f;					// Min battery temperature where charging is still allowed
	configLocation->allowedTempBattCoolingMax			= 5.0f;					// Max battery temperature where cooling is activated
	configLocation->allowedTempBattCoolingMin			= 50.0f;				// Min battery temperature where heating is activated
	configLocation->allowedTempBMSMax				= 80.0f;				// Max BMS operational temperature
	configLocation->allowedTempBMSMin				= 0.0f;					// Min BMS operational temperature
	configLocation->displayTimeoutBatteryDead			= 5000;					// Show battery dead symbol X seconds before going to powerdown in cell voltage error state.
	configLocation->displayTimeoutBatteryError			= 5000;					// Show error symbol for X seconds before going to powerdown in general error state.
	configLocation->displayTimeoutBatteryErrorPreCharge		= 10000;				// Show pre charge error for XX seconds.
	configLocation->displayTimeoutSplashScreen			= 3000;					// Display / INIT splash screen time.
	configLocation->displayStyle					= advanced;				// Display style used for showing the SSD1306 data
	configLocation->maxUnderAndOverVoltageErrorCount		= 5;					// Max count of hard cell voltage errors.
	configLocation->maxUnderAndOverTemperatureErrorCount		= 5;					// Max count of hard cell voltage errors.
	configLocation->notUsedCurrentThreshold				= 1.0f;					// If abs(packcurrent) < X.XA consider pack as not used.
	configLocation->notUsedTimeout					= 20*60*1000;				// If pack is not used for longer than XX minutes disable bms.
	configLocation->stateOfChargeStoreInterval			= 60*1000;				// Interval in ms to store state of charge information.
	configLocation->stateOfChargeMethod				= socCoulomb;				// Use coulomb counting for SoC calculation
	configLocation->CANID						= 10;					// CAN ID for CAN communication.
	configLocation->CANIDStyle					= CANIDStyleVESC;			// CAN ID default Style.
	configLocation->canBusSpeed					= canSpeedBaud500k;			// 500k CAN baud
	configLocation->emitStatusOverCAN				= false;				// Send status over can.
	configLocation->emitStatusProtocol				= canEmitProtocolVESC;			// Can emit protocol set to MG style for backwards compatibility
	configLocation->tempEnableMaskBMS				= 0x0001;				// Bitwise select what sensor to enable for the BMS (internal sensors).
	configLocation->tempEnableMaskBattery				= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
  	configLocation->tempEnableMaskExpansion				= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
	configLocation->noOfTempSensorPerModule				= 0;					// Number of temperature sensors monitored per LTC68XX
	configLocation->noOfExpansionBoard				= 0;					// Number of expansion board
	configLocation->noOfTempSensorPerExpansionBoard			= 0;					// Number of temperature sensors monitored per expansion board
	configLocation->LCUseDischarge					= disabled;				// Enable or disable the solid state output
	configLocation->LCUsePrecharge					= disabled;				// Use precharge before enabling main output
	configLocation->allowChargingDuringDischarge			= false;				// Allow the battery to be charged in normal mode
	configLocation->allowForceOn					= false;				// Allow the BMS to be forced ON by long actuation of the power button
	configLocation->pulseToggleButton				= true;					// Select either pulse or toggle power button
	configLocation->useCANSafetyInput				= false;				// Use the safety input status from CAN
	configLocation->useCANDelayedPowerDown				= false;				// Use delayed power down
	configLocation->NTCTopResistor[modConfigNTCGroupLTCExt]		= 100000;				// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupMasterPCB]	= 100000;				// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupExp]		= 100000;				// NTC Pullup resistor value
	configLocation->NTC25DegResistance[modConfigNTCGroupLTCExt]	= 100000;				// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupMasterPCB]	= 100000;				// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupExp]	= 100000;				// NTC resistance at 25 degree
	configLocation->NTCBetaFactor[modConfigNTCGroupLTCExt]		= 4250;					// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupMasterPCB]	= 4250;					// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupExp]		= 4250;					// NTC Beta factor
	configLocation->cellMonitorType					= CELL_MON_LTC6813_1;			// Use the new cell voltage monitor
	configLocation->cellMonitorICCount				= 1;					// Only one slave IC
	configLocation->externalEnableOperationalState			= opStateExtNormal;			// Go to normal enable mode
	configLocation->chargeEnableOperationalState			= opStateChargingModeCharging;		// Go to charging mode when a charger is connected
	configLocation->powerDownDelay					= 3000;					// Wait only minimal to turn off
	configLocation->noOfCellsPerModule				= 18;					// Number of cell levels monitored per LTC68XX	
	configLocation->lastICNoOfCells					= 0;
	configLocation->lastICMask					= 0;
	configLocation->humidityICType					= 2;
	configLocation->BMSApplication					= electricVehicle;
	
#elif ENNOID_XLITE 
	configLocation->noOfCellsSeries					= 24;					// Total number of cells in series in the battery pack
	configLocation->noOfCellsParallel				= 10;					// Number of cells in parallel
	configLocation->noOfParallelModules				= 1;					// Number of parallel modules
	configLocation->batteryCapacity					= 22.00f;				// XXAh battery
	configLocation->cellHardUnderVoltage				= 2.30f;				// Worst case X.XXV as lowest cell voltage
	configLocation->cellHardOverVoltage				= 4.20f;				// Worst case X.XXV as highest cell voltage
	configLocation->cellLCSoftUnderVoltage				= 2.70f;				// Lowest cell voltage X.XXV.
	configLocation->cellSoftOverVoltage				= 4.15f;				// Normal highest cell voltage X.XXV.
	configLocation->cellBalanceDifferenceThreshold			= 0.01f;				// Start balancing @ XmV difference, stop if below.
	configLocation->cellBalanceStart				= 4.1f;					// Start balancing above X.XXV.
	configLocation->cellBalanceAllTime				= false;				// Enable balancing under all opstate
	configLocation->cellThrottleUpperStart				= 0.03f;				// Upper range of cell voltage for charge throttling.
	configLocation->cellThrottleLowerStart				= 0.20f;				// Lower range of cell voltage for discharge throttling.
	configLocation->cellThrottleUpperMargin				= 0.01f;				// Margin of throttle from upper soft limits.
	configLocation->cellThrottleLowerMargin				= 0.50f;				// Margin of throttle from lower soft limits.	
	configLocation->packVoltageDataSource				= sourcePackVoltageISL28022;		// Packvoltage source.
	configLocation->packCurrentDataSource				= sourcePackCurrentISL28022;		// The pack current is the same as the current through the low current shunt
	configLocation->buzzerSignalSource				= buzzerSourceOn;			// Stores what source shoud be taken to trigger
	configLocation->buzzerSignalPersistant				= false;				// Stores whether the buzzer should stay on after triggering
	configLocation->shuntLCFactor					= -0.00043f;              		// Shunt factor low current
	configLocation->voltageLCFactor					= 2.75f;				// Pack voltage factor 
	configLocation->voltageLCOffset					= 0;					// Pack voltage offset 
	configLocation->loadVoltageFactor				= 41.0f;				// Load voltage factor 
	configLocation->loadVoltageOffset				= 0.0f;					// Load voltage offset
	configLocation->chargerVoltageFactor				= 41.0f;				// Charger voltage factor 
	configLocation->chargerVoltageOffset				= 0.0f;					// Charger voltage offset
	configLocation->throttleChargeIncreaseRate			= 1;					// Percentage charge throttle increase rate per 100ms (cell voltage loop time)  
	configLocation->throttleDisChargeIncreaseRate			= 2;					// Percentage discharge throttle increase rate per 100ms (cell voltage loop time)  	
	configLocation->cellBalanceUpdateInterval			= 4*1000;				// Keep calculated resistors enabled for this amount of time in miliseconds.
	configLocation->maxSimultaneousDischargingCells			= 5;					// Allow a maximum of X cells simultinous discharging trough bleeding resistors.
	configLocation->timeoutDischargeRetry				= 10*1000;				// Wait for X seconds before retrying to enable load.
	configLocation->hysteresisDischarge				= 0.02f;				// Lowest cell should rise XXmV before output is re enabled.
	configLocation->timeoutChargeRetry				= 30*1000;				// Wait for XX seconds before retrying to enable charger.
	configLocation->hysteresisCharge				= 0.01f;				// Highest cell should lower XXmV before charger is re enabled.
	configLocation->timeoutChargeCompleted				= 30*60*1000;				// Wait for XX minutes before setting charge state to charged.
	configLocation->timeoutChargingCompletedMinimalMismatch		= 6*1000;				// If cell mismatch is under threshold and (charging is not allowed) wait this delay time to set "charged" state.
	configLocation->maxMismatchThreshold				= 0.010f;				// If mismatch is under this threshold for timeoutChargingCompletedMinimalMismatch determin fully charged.
	configLocation->chargerEnabledThreshold				= 0.5f;					// If charge current > X.XA stay in charging mode and dont power off.
	configLocation->timeoutChargerDisconnected			= 2000;					// Wait for X seconds to respond to charger disconnect.
	configLocation->minimalPrechargePercentage			= 0.70f;				// output should be at a minimal of 80% of input voltage.
	configLocation->timeoutLCPreCharge				= 1.5*1000;				// Precharge error timeout, allow 1.5 seconds pre-charge time before declaring load error.
	configLocation->maxAllowedCurrent				= 1000.0f;				// Allow max XXXA trough BMS.
	configLocation->allowedTempBattDischargingMax			= 105.0f;				// Max battery temperature where discharging is still allowed
	configLocation->allowedTempBattDischargingMin			= 0.0f;					// Min battery temperature where discharging is still allowed
	configLocation->allowedTempBattChargingMax			= 105.0f;				// Max battery temperature where charging is still allowed
	configLocation->allowedTempBattChargingMin			= 0.0f;					// Min battery temperature where charging is still allowed
	configLocation->allowedTempBattCoolingMax			= 5.0f;					// Max battery temperature where cooling is activated
	configLocation->allowedTempBattCoolingMin			= 50.0f;				// Min battery temperature where heating is activated
	configLocation->allowedTempBMSMax				= 80.0f;				// Max BMS operational temperature
	configLocation->allowedTempBMSMin				= 0.0f;					// Min BMS operational temperature
	configLocation->displayTimeoutBatteryDead			= 5000;					// Show battery dead symbol X seconds before going to powerdown in cell voltage error state.
	configLocation->displayTimeoutBatteryError			= 5000;					// Show error symbol for X seconds before going to powerdown in general error state.
	configLocation->displayTimeoutBatteryErrorPreCharge		= 10000;				// Show pre charge error for XX seconds.
	configLocation->displayTimeoutSplashScreen			= 3000;					// Display / INIT splash screen time.
	configLocation->displayStyle					= advanced;				// Display style used for showing the SSD1306 data
	configLocation->maxUnderAndOverVoltageErrorCount		= 5;					// Max count of hard cell voltage errors.
	configLocation->maxUnderAndOverTemperatureErrorCount		= 5;					// Max count of hard cell voltage errors.
	configLocation->notUsedCurrentThreshold				= 1.0f;					// If abs(packcurrent) < X.XA consider pack as not used.
	configLocation->notUsedTimeout					= 20*60*1000;				// If pack is not used for longer than XX minutes disable bms.
	configLocation->stateOfChargeStoreInterval			= 60*1000;				// Interval in ms to store state of charge information.
	configLocation->stateOfChargeMethod				= socCoulomb;				// Use coulomb counting for SoC calculation
	configLocation->CANID						= 10;					// CAN ID for CAN communication.
	configLocation->CANIDStyle					= CANIDStyleVESC;			// CAN ID default Style.
	configLocation->canBusSpeed					= canSpeedBaud500k;			// 500k CAN baud
	configLocation->emitStatusOverCAN				= false;				// Send status over can.
	configLocation->emitStatusProtocol				= canEmitProtocolVESC;			// Can emit protocol set to MG style for backwards compatibility
	configLocation->tempEnableMaskBMS				= 0x0001;				// Bitwise select what sensor to enable for the BMS (internal sensors).
	configLocation->tempEnableMaskBattery				= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
  	configLocation->tempEnableMaskExpansion				= 0xFFFF;				// Bitwise select what sensor to enable for the battery (external sensors).
	configLocation->noOfTempSensorPerModule				= 0;					// Number of temperature sensors monitored per LTC68XX
	configLocation->noOfExpansionBoard				= 0;					// Number of expansion board
	configLocation->noOfTempSensorPerExpansionBoard			= 0;					// Number of temperature sensors monitored per expansion board
	configLocation->LCUseDischarge					= disabled;				// Enable or disable the solid state output
	configLocation->LCUsePrecharge					= disabled;				// Use precharge before enabling main output
	configLocation->allowChargingDuringDischarge			= false;				// Allow the battery to be charged in normal mode
	configLocation->allowForceOn					= false;				// Allow the BMS to be forced ON by long actuation of the power button
	configLocation->pulseToggleButton				= true;					// Select either pulse or toggle power button
	configLocation->useCANSafetyInput				= false;				// Use the safety input status from CAN
	configLocation->useCANDelayedPowerDown				= false;				// Use delayed power down
	configLocation->NTCTopResistor[modConfigNTCGroupLTCExt]		= 100000;				// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupMasterPCB]	= 100000;				// NTC Pullup resistor value
	configLocation->NTCTopResistor[modConfigNTCGroupExp]		= 100000;				// NTC Pullup resistor value
	configLocation->NTC25DegResistance[modConfigNTCGroupLTCExt]	= 100000;				// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupMasterPCB]	= 100000;				// NTC resistance at 25 degree
	configLocation->NTC25DegResistance[modConfigNTCGroupExp]	= 100000;				// NTC resistance at 25 degree
	configLocation->NTCBetaFactor[modConfigNTCGroupLTCExt]		= 4250;					// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupMasterPCB]	= 4250;					// NTC Beta factor
	configLocation->NTCBetaFactor[modConfigNTCGroupExp]		= 4250;					// NTC Beta factor
	configLocation->cellMonitorType					= CELL_MON_LTC6811_1;			// Use the new cell voltage monitor
	configLocation->cellMonitorICCount				= 2;					// Only one slave IC
	configLocation->externalEnableOperationalState			= opStateExtNormal;			// Go to normal enable mode
	configLocation->chargeEnableOperationalState			= opStateChargingModeCharging;		// Go to charging mode when a charger is connected
	configLocation->powerDownDelay					= 3000;					// Wait only minimal to turn off
	configLocation->noOfCellsPerModule				= 18;					// Number of cell levels monitored per LTC68XX	
	configLocation->lastICNoOfCells					= 0;
	configLocation->lastICMask					= 0;
	configLocation->humidityICType					= 2;
	configLocation->BMSApplication					= electricVehicle;
	
#endif

    // Existing board presets keep conservative output behavior: CAN status off.
    configLocation->maxAllowedChargeCurrent = configLocation->maxAllowedCurrent;
    configLocation->loadCANProtocol = loadCANProtocolNone;
    configLocation->chargerCANProtocol = chargerCANProtocolNone;
    modConfigSyncLegacyCAN(configLocation);

}


void modConfigSyncLegacyCAN(modConfigGeneralConfigStructTypedef *config) {
    config->emitStatusOverCAN = config->loadCANProtocol != loadCANProtocolNone;
    config->emitStatusProtocol = config->loadCANProtocol == loadCANProtocolVESC ?
        canEmitProtocolVESC : config->loadCANProtocol;
}

bool modConfigValidate(modConfigGeneralConfigStructTypedef *c) {
    if (!c) return false;
    // Checking every serialized float also catches overflow in float32_auto decoding.
#define FINITE_F32(member) if (!isfinite(c->member)) return false
#define FINITE_U8(member)
#define FINITE_BOOL(member)
#define FINITE_U16(member)
#define FINITE_I16(member)
#define FINITE_U32(member)
#define FINITE(kind, member, wire_name) FINITE_##kind(member);
    MOD_CONFIG_WIRE_FIELDS(FINITE)
#undef FINITE
    if (c->noOfCellsParallel == 0 || c->batteryCapacity <= 0.0f ||
        c->maxAllowedCurrent <= 0.0f || c->maxAllowedChargeCurrent <= 0.0f ||
        c->cellHardUnderVoltage <= 0.0f || c->cellHardOverVoltage > 10.0f ||
        c->cellHardUnderVoltage >= c->cellHardOverVoltage ||
        c->cellLCSoftUnderVoltage < c->cellHardUnderVoltage ||
        c->cellSoftOverVoltage > c->cellHardOverVoltage ||
        c->cellLCSoftUnderVoltage >= c->cellSoftOverVoltage ||
        c->cellBalanceStart <= 0.0f || c->cellBalanceStart > c->cellHardOverVoltage ||
        c->cellBalanceDifferenceThreshold < 0.0f ||
        c->cellThrottleUpperStart < 0.0f || c->cellThrottleLowerStart < 0.0f ||
        c->cellThrottleUpperMargin < 0.0f || c->cellThrottleLowerMargin < 0.0f ||
        c->shuntLCFactor == 0.0f || c->voltageLCFactor <= 0.0f ||
        c->loadVoltageFactor <= 0.0f || c->chargerVoltageFactor <= 0.0f ||
        c->hysteresisCharge < 0.0f || c->hysteresisDischarge < 0.0f ||
        c->maxMismatchThreshold < 0.0f || c->chargerEnabledThreshold < 0.0f ||
        c->minimalPrechargePercentage <= 0.0f || c->minimalPrechargePercentage > 1.0f ||
        c->notUsedCurrentThreshold < 0.0f || c->cellBalanceUpdateInterval == 0 ||
        c->stateOfChargeStoreInterval == 0) return false;

    if (c->loadCANProtocol > loadCANProtocolVESC ||
        (c->chargerCANProtocol != chargerCANProtocolNone &&
         c->chargerCANProtocol != chargerCANProtocolDeltaQ) ||
        c->CANIDStyle > CANIDStyleFoiler || c->canBusSpeed > canSpeedBaud500k ||
        (c->packVoltageDataSource != sourcePackVoltageISL28022 &&
         c->packVoltageDataSource != sourcePackVoltageSumOfIndividualCellVoltages) ||
        c->packCurrentDataSource != sourcePackCurrentISL28022 ||
        c->buzzerSignalSource > buzzerSourceSOA || c->displayStyle > 3 ||
        c->humidityICType > htc1080 || c->cellTypeUsed != 0 ||
        c->stateOfChargeMethod != socCoulomb ||
        c->externalEnableOperationalState > opStateExtNormal ||
        c->chargeEnableOperationalState > opStateChargingModeNormal ||
        c->BMSApplication > energyStorage) return false;

#define CHECK_BOOL(member) if (c->member > 1) return false
    CHECK_BOOL(buzzerSignalPersistant); CHECK_BOOL(allowChargingDuringDischarge);
    CHECK_BOOL(allowForceOn); CHECK_BOOL(pulseToggleButton);
    CHECK_BOOL(useCANSafetyInput); CHECK_BOOL(useCANDelayedPowerDown);
    if (c->LCUseDischarge > forced || c->LCUsePrecharge > forced) return false;
#undef CHECK_BOOL

    const float temps[] = { c->allowedTempBattDischargingMax, c->allowedTempBattDischargingMin,
        c->allowedTempBattChargingMax, c->allowedTempBattChargingMin,
        c->allowedTempBattCoolingMax, c->allowedTempBattCoolingMin,
        c->allowedTempBMSMax, c->allowedTempBMSMin };
    for (unsigned i = 0; i < sizeof(temps)/sizeof(temps[0]); ++i)
        if (temps[i] < -100.0f || temps[i] > 200.0f) return false;
    if (c->allowedTempBattDischargingMin >= c->allowedTempBattDischargingMax ||
        c->allowedTempBattChargingMin >= c->allowedTempBattChargingMax ||
        c->allowedTempBMSMin >= c->allowedTempBMSMax) return false;
    // Fixed sensor storage and per-monitor GPIO/reference channel limits.
    unsigned auxCapacity = c->cellMonitorType == CELL_MON_LTC6811_1 ? 5U : 9U;
    if ((c->tempEnableMaskBMS & ~0x3FFFUL) != 0 ||
        c->noOfTempSensorPerModule > auxCapacity ||
        c->noOfExpansionBoard > 9 || c->noOfTempSensorPerExpansionBoard > 8 ||
        (unsigned)c->noOfExpansionBoard * c->noOfTempSensorPerExpansionBoard > 72)
        return false;
    for (unsigned i = modConfigNTCGroupLTCExt; i <= modConfigNTCGroupExp; ++i)
        if (!c->NTCTopResistor[i] || !c->NTC25DegResistance[i] || !c->NTCBetaFactor[i])
            return false;
    return modconfigHardwareLimitsApply(c);
}

// Migration offsets are intentionally tied to the two audited ARM ABI layouts.
_Static_assert(offsetof(modConfigGeneralConfigStructTypedef, humidityICType) == 310,
               "legacy config prefix changed; update explicit migration");
_Static_assert(offsetof(modConfigGeneralConfigStructTypedef, BMSApplication) == 313,
               "legacy316 tail changed; update explicit migration");
_Static_assert(offsetof(modConfigGeneralConfigStructTypedef, maxAllowedChargeCurrent) == 316,
               "new fields must remain appended after legacy316");

bool modConfigMigrateLegacy(void *dest, const uint8_t *raw, uint16_t rawBytes) {
    if (!dest || !raw || (rawBytes != 312 && rawBytes != 316)) return false;
    // Both known layouts have their sole C bool at byte32; reject invalid representation.
    if (raw[offsetof(modConfigGeneralConfigStructTypedef, cellBalanceAllTime)] > 1)
        return false;
    modConfigGeneralConfigStructTypedef next;
    memset(&next, 0, sizeof(next));
    memcpy(&next, raw, rawBytes);
    if (rawBytes == 312) {
        next.BMSApplication = raw[311];
        next.cellTypeUsed = 0;
        next.firstICNoCells = 0;
    }
    if (next.emitStatusOverCAN > 1 || next.emitStatusProtocol > canEmitProtocolVESC)
        return false;
    if (!next.emitStatusOverCAN || next.emitStatusProtocol == canEmitProtocolNone)
        next.loadCANProtocol = loadCANProtocolNone;
    else if (next.emitStatusProtocol == canEmitProtocolVESC)
        next.loadCANProtocol = loadCANProtocolVESC;
    else if (next.emitStatusProtocol == canEmitProtocolDieBieEngineering)
        next.loadCANProtocol = loadCANProtocolDieBieEngineering;
    else return false; // No implemented MG protocol to migrate safely.
    next.chargerCANProtocol = chargerCANProtocolNone;
    // Old firmware used the one magnitude limit for charge and discharge.
    next.maxAllowedChargeCurrent = next.maxAllowedCurrent;
    if (!modConfigValidate(&next)) return false;
    modConfigSyncLegacyCAN(&next);
    *(modConfigGeneralConfigStructTypedef *)dest = next;
    return true;
}

void modConfigPublish(modConfigGeneralConfigStructTypedef *dest,
                      const modConfigGeneralConfigStructTypedef *src) {
#if defined(__arm__) || defined(__thumb__)
    // RX IRQs can consult configuration; do not expose a partly copied struct.
    uint32_t mask;
    __asm volatile ("mrs %0, primask" : "=r" (mask));
    __asm volatile ("cpsid i" ::: "memory");
    *dest = *src;
    __asm volatile ("msr primask, %0" :: "r" (mask) : "memory");
#else
    *dest = *src;
#endif
}
