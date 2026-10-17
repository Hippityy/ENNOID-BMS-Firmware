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

#include "modCommands.h"
#include "modConfigWire.h"
#include "modCommandPolicy.h"
#include "confxml.h"
#include "confparser.h"
#include "libMempools.h"
#include "conf_default.h"


// Private variables
static uint8_t modCommandsSendBuffer[PACKET_MAX_PL_LEN];
static void(*modCommandsSendFunction)(unsigned char *data, unsigned int len) = 0;
bool jumpBootloaderTrue;
static bool modCommandsCommissioning;
static bool modCommandsHasPendingConfig;
static modConfigGeneralConfigStructTypedef modCommandsPendingConfig;
static uint32_t modCommandsBootloaderDelayTick;
static void modCommandsConfigToVESC(main_config_t *conf,
    const modConfigGeneralConfigStructTypedef *config);
modConfigGeneralConfigStructTypedef *modCommandsGeneralConfig;
modConfigGeneralConfigStructTypedef *modCommandsToBeSendConfig;
modConfigGeneralConfigStructTypedef  modCommandsConfigStorage;
modPowerElectronicsPackStateTypedef *modCommandsGeneralState;

void modCommandsInit(modPowerElectronicsPackStateTypedef   *generalState,modConfigGeneralConfigStructTypedef *configPointer) {
	modCommandsGeneralConfig = configPointer;
	modCommandsGeneralState  = generalState;
	jumpBootloaderTrue = false;
    modCommandsHasPendingConfig = false;
}

bool modCommandsIsCommissioningMode(void) {
    return modCommandsCommissioning;
}

void modCommandsSetCommissioningMode(bool enabled) {
    modCommandsCommissioning = enabled;
}

void modCommandsSetSendFunction(void(*func)(unsigned char *data, unsigned int len)) {
	modCommandsSendFunction = func;
}

void modCommandsSendPacket(unsigned char *data, unsigned int len) {
	if (modCommandsSendFunction) {
		modCommandsSendFunction(data, len);
	}
}

void modCommandsProcessPacket(unsigned char *data, unsigned int len) {
	if (!data || !len || len > PACKET_MAX_PL_LEN) {
		return;
	}

	COMM_PACKET_ID packet_id;
	int32_t ind = 0;
	uint16_t flash_res;
	uint32_t new_app_offset;
	uint8_t cellPointer;
	uint8_t auxPointer;
	uint8_t expPointer;
	uint8_t totalNoOfCells;
	uint8_t totalNoOfAux;

	packet_id = (COMM_PACKET_ID) data[0];
	data++;
	len--;
    if (!modCommandPolicyAllows(packet_id, data, len, modCommandsCommissioning)) return;

	switch (packet_id) {
		case COMM_FW_VERSION:
			ind = 0;
			modCommandsSendBuffer[ind++] = COMM_FW_VERSION;
			modCommandsSendBuffer[ind++] = FW_VERSION_MAJOR;
			modCommandsSendBuffer[ind++] = FW_VERSION_MINOR;
			strcpy((char*)(modCommandsSendBuffer + ind), HW_NAME);
			ind += strlen(HW_NAME) + 1;
			memcpy(modCommandsSendBuffer + ind, STM32_UUID_8, 12);
			ind += 12;
			modCommandsSendBuffer[ind++] = 0;
			modCommandsSendBuffer[ind++] = FW_TEST_VERSION_NUMBER;

			modCommandsSendBuffer[ind++] = HW_TYPE_VESC_BMS;

			modCommandsSendBuffer[ind++] = 1; // One custom config

			modCommandsSendPacket(modCommandsSendBuffer, ind);
			break;
		case COMM_JUMP_TO_BOOTLOADER:
			jumpBootloaderTrue = true;
			modCommandsBootloaderDelayTick = HAL_GetTick();
			break;
		case COMM_ERASE_NEW_APP:
			ind = 0;
			flash_res = modFlashEraseNewAppData(libBufferGet_uint32(data, &ind));

			ind = 0;
			modCommandsSendBuffer[ind++] = COMM_ERASE_NEW_APP;
			modCommandsSendBuffer[ind++] = flash_res == HAL_OK ? true : false;
			modCommandsSendPacket(modCommandsSendBuffer, ind);
			break;
		case COMM_WRITE_NEW_APP_DATA:
			ind = 0;
			new_app_offset = libBufferGet_uint32(data, &ind);
			flash_res = modFlashWriteNewAppData(new_app_offset, data + ind, len - ind);

			ind = 0;
			modCommandsSendBuffer[ind++] = COMM_WRITE_NEW_APP_DATA;
			modCommandsSendBuffer[ind++] = flash_res == HAL_OK ? 1 : 0;
			modCommandsSendPacket(modCommandsSendBuffer, ind);
			break;
		case COMM_EBMS_GET_VALUES:
			ind = 0;
			modCommandsSendBuffer[ind++] = COMM_EBMS_GET_VALUES;
		
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->packVoltage,			1e3, 		&ind);
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->packCurrent,			1e3,		&ind);
		  	libBufferAppend_uint8(modCommandsSendBuffer, (uint8_t)round(modCommandsGeneralState->SoC), 				&ind);
		  	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->cellVoltageHigh,		1e3,		&ind);
		  	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->cellVoltageAverage,		1e3,		&ind);
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->cellVoltageLow,			1e3,		&ind);
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->cellVoltageMisMatch,		1e3,		&ind);
		  	libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->loCurrentLoadVoltage,		1e1,		&ind);
		  	libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->loCurrentLoadCurrent,		1e1,		&ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->chargerVoltage,			1e1,		&ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->tempBatteryHigh,		1e1,		&ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->tempBatteryAverage,		1e1,		&ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->tempBatteryLow,			1e1,		&ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->tempBMSHigh,			1e1,		&ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->tempBMSAverage,			1e1,		&ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->tempBMSLow,			1e1,		&ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->humidity,			1e1,		&ind);
			libBufferAppend_uint8(modCommandsSendBuffer, (uint8_t)modCommandsGeneralState->operationalState,			&ind);
			libBufferAppend_uint8(modCommandsSendBuffer, (uint8_t)modCommandsGeneralState->chargeBalanceActive,			&ind);
			libBufferAppend_uint8(modCommandsSendBuffer, (uint8_t)modCommandsGeneralState->faultState,				&ind);
			libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->AhCounter,				1e3,		&ind);
			libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->WhCounter,				1e3,		&ind);
			libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->AhCounterCharge,		1e3,		&ind);
			libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->WhCounterCharge,		1e3,		&ind);
			libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->AhCounterDischarge,		1e3,		&ind);
			libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->WhCounterDischarge,		1e3,		&ind);
		
			modCommandsSendBuffer[ind++] = modCommandsGeneralConfig->CANID;
			modCommandsSendPacket(modCommandsSendBuffer, ind);
		
			break;
    		case COMM_EBMS_GET_CELLS:
			ind = 0;
			modCommandsSendBuffer[ind++] = COMM_EBMS_GET_CELLS;
		
		  	libBufferAppend_uint8(modCommandsSendBuffer, modCommandsGeneralConfig->noOfCellsSeries*modCommandsGeneralConfig->noOfParallelModules, &ind);                
		  	for(cellPointer = 0; cellPointer < modCommandsGeneralConfig->noOfCellsSeries*modCommandsGeneralConfig->noOfParallelModules; cellPointer++){
				if(modCommandsGeneralState->cellVoltagesIndividual[cellPointer].cellBleedActive)
					libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->cellVoltagesIndividual[cellPointer].cellVoltage*-1.0f, 1e3, &ind);    
				else
					libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->cellVoltagesIndividual[cellPointer].cellVoltage, 1e3, &ind);          
			}
		
			modCommandsSendBuffer[ind++] = modCommandsGeneralConfig->CANID;
			modCommandsSendPacket(modCommandsSendBuffer, ind);
			break;
		case COMM_EBMS_GET_AUX:
			ind = 0;
			modCommandsSendBuffer[ind++] = COMM_EBMS_GET_AUX;
		
		  	libBufferAppend_uint8(modCommandsSendBuffer, modCommandsGeneralConfig->cellMonitorICCount*modCommandsGeneralConfig->noOfTempSensorPerModule, &ind);                
		  	for(auxPointer = 0; auxPointer < modCommandsGeneralConfig->cellMonitorICCount*modCommandsGeneralConfig->noOfTempSensorPerModule; auxPointer++){
					libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->auxVoltagesIndividual[auxPointer].auxVoltage, 1e1, &ind);          
			}
		
			modCommandsSendBuffer[ind++] = modCommandsGeneralConfig->CANID;
			modCommandsSendPacket(modCommandsSendBuffer, ind);
			break;
		case COMM_EBMS_GET_EXP_TEMP:
			ind = 0;
			modCommandsSendBuffer[ind++] = COMM_EBMS_GET_EXP_TEMP;
		
		  	libBufferAppend_uint8(modCommandsSendBuffer, modCommandsGeneralConfig->noOfExpansionBoard*modCommandsGeneralConfig->noOfTempSensorPerExpansionBoard, &ind);
		  	for(expPointer = 0; expPointer < modCommandsGeneralConfig->noOfExpansionBoard*modCommandsGeneralConfig->noOfTempSensorPerExpansionBoard; expPointer++){
					libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->expVoltagesIndividual[expPointer].expVoltage, 1e1, &ind);
			}
		
			modCommandsSendBuffer[ind++] = modCommandsGeneralConfig->CANID;
			modCommandsSendPacket(modCommandsSendBuffer, ind);
			break;
		case COMM_EBMS_SET_MCCONF:
            if (!modCommandsHasPendingConfig) modCommandsPendingConfig = *modCommandsGeneralConfig;
            if (!modConfigWireDecode(&modCommandsPendingConfig, data, len)) break;
            modCommandsHasPendingConfig = true;
            modCommandsSendBuffer[0] = packet_id;
            modCommandsSendPacket(modCommandsSendBuffer, 1);
            break;
        case COMM_EBMS_GET_MCCONF:
        case COMM_EBMS_GET_MCCONF_DEFAULT:
            if (len != 0) break;
            if (packet_id == COMM_EBMS_GET_MCCONF_DEFAULT) {
                modConfigLoadDefaultConfig(&modCommandsConfigStorage);
                if (!modConfigValidate(&modCommandsConfigStorage)) break;
                modCommandsToBeSendConfig = &modCommandsConfigStorage;
            } else {
                modCommandsToBeSendConfig = modCommandsHasPendingConfig ?
                    &modCommandsPendingConfig : modCommandsGeneralConfig;
            }
            modCommandsSendBuffer[0] = packet_id;
            ind = modConfigWireEncode(modCommandsSendBuffer + 1,
                sizeof(modCommandsSendBuffer) - 1, modCommandsToBeSendConfig);
            if (ind == MOD_CONFIG_WIRE_SIZE)
                modCommandsSendPacket(modCommandsSendBuffer, ind + 1);
            break;
        case COMM_TERMINAL_CMD: {
            char terminal[128];
            memcpy(terminal, data, len);
            terminal[len] = '\0';
            modTerminalProcessString(terminal);
            break;
        }
		case COMM_REBOOT:
			modCommandsJumpToMainApplication();
			break;
		case COMM_ALIVE:
			break;
		case COMM_FORWARD_CAN:
			modCANSendBuffer(data[0], data + 1, len - 1, false);
			break;
		case COMM_EBMS_STORE_CONF:
            if (len != 0) break;
            if (modCommandsHasPendingConfig) {
                if (!modConfigStoreCandidate(&modCommandsPendingConfig)) break;
                modCommandsSetCommissioningMode(true);
            } else if (!modConfigStoreConfig()) break;
		
			ind = 0;
			modCommandsSendBuffer[ind++] = packet_id;
			modCommandsSendPacket(modCommandsSendBuffer, ind);
			break;
		//Specific to VESC tool app
		case COMM_BMS_GET_VALUES: 
			ind = 0;

			modCommandsSendBuffer[ind++] = COMM_BMS_GET_VALUES;

		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->packVoltage,			1e6, 		&ind);
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->chargerVoltage,			1e6, 		&ind);
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->packCurrent,			1e6, 		&ind);
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->packCurrent,			1e6, 		&ind);
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->packCurrent,			1e3, 		&ind); //TO DO: define AhCounter instead of packCurrent
		 	libBufferAppend_float32(modCommandsSendBuffer, modCommandsGeneralState->packVoltage,			1e3, 		&ind); //TO DO: define WhCounter instead of packCurrent

			// Cell voltages
			totalNoOfCells = modCommandsGeneralConfig->noOfCellsSeries*modCommandsGeneralConfig->noOfParallelModules;
			modCommandsSendBuffer[ind++] = totalNoOfCells;
			for (cellPointer = 0; cellPointer < totalNoOfCells; cellPointer++) {
				libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->cellVoltagesIndividual[cellPointer].cellVoltage, 1e3, &ind);
			}

			// Balancing state
			for (int i = 0; i < totalNoOfCells; i++) {
				modCommandsSendBuffer[ind++] = modCommandsGeneralState->cellVoltagesIndividual[i].cellBleedActive;
			}

			// Temperatures
			totalNoOfAux = modCommandsGeneralConfig->cellMonitorICCount*modCommandsGeneralConfig->noOfTempSensorPerModule;
			modCommandsSendBuffer[ind++] = totalNoOfAux;
			for (auxPointer = 0; auxPointer < totalNoOfAux; auxPointer++) {
				libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->auxVoltagesIndividual[auxPointer].auxVoltage, 1e2, &ind); 
			}
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->temperatures[0], 1e2, &ind); 

			// Humidity
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->humidity, 1e2, &ind);
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->temperatures[1], 1e2, &ind);

			// Highest cell temperature
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->tempBatteryHigh, 1e2, &ind);

			// State of charge and state of health
			libBufferAppend_float16(modCommandsSendBuffer, modCommandsGeneralState->SoC/100, 1e3, &ind);
			libBufferAppend_float16(modCommandsSendBuffer, 0.0, 1e3, &ind);

		  	modCommandsSendPacket(modCommandsSendBuffer, ind);
			break;

		case COMM_BMS_SET_CHARGE_ALLOWED:
			modCommandsGeneralState->chargeAllowed = true;
			break;

		case COMM_BMS_SET_BALANCE_OVERRIDE:
			//modCommandsGeneralConfig->cellBalanceAllTime = true;
			break;

		case COMM_BMS_RESET_COUNTERS:
			if (data[0]) {
			//To do
			}

			if (data[1]) {
			//To do
			}
			break;

		case COMM_BMS_FORCE_BALANCE:
			modCommandsGeneralConfig->cellBalanceAllTime = true;
			break;
		case COMM_BMS_ZERO_CURRENT_OFFSET:
			modPowerElectronicsResetCurrentOffset();
		break;
		case COMM_GET_CUSTOM_CONFIG:
		case COMM_GET_CUSTOM_CONFIG_DEFAULT: {
            int conf_ind = data[0];
            if (conf_ind != 0) break;
            main_config_t *conf = libMempools_alloc_conf();
            if (!conf) break;

			if (packet_id == COMM_GET_CUSTOM_CONFIG) {
				modCommandsEBMSToVESC(conf);
			} else {
                modConfigLoadDefaultConfig(&modCommandsConfigStorage);
                modCommandsConfigToVESC(conf, &modCommandsConfigStorage);
			}

			ind = 0;
			modCommandsSendBuffer[ind++] = packet_id;
			modCommandsSendBuffer[ind++] = conf_ind;
			int32_t len = confparser_serialize_main_config_t(modCommandsSendBuffer + ind, conf);
			modCommandsSendPacket(modCommandsSendBuffer, len + ind);
			libMempools_free_conf(conf);
		} break;

		case COMM_SET_CUSTOM_CONFIG: {
			main_config_t *conf = libMempools_alloc_conf();

			int conf_ind = data[0];

            if (!conf) break;
            if (conf_ind == 0 && confparser_deserialize_main_config_t(data + 1, conf) &&
                modCommandsVESCToEBMS(conf)) {

				ind = 0;
				//modCommandsSendBuffer[50];
				modCommandsSendBuffer[ind++] = packet_id;
				modCommandsSendPacket(modCommandsSendBuffer, ind);
			} else {
				modCommandsPrintf("Warning: Could not set configuration");
			}

			libMempools_free_conf(conf);
		} break;

		case COMM_GET_CUSTOM_CONFIG_XML: {
			ind = 0;

			int conf_ind = data[ind++];

			if (conf_ind != 0) {
				break;
			}

			int32_t len_conf = libBufferGet_int32(data, &ind);
			int32_t ofs_conf = libBufferGet_int32(data, &ind);

			if (len_conf < 0 || ofs_conf < 0 || ofs_conf > DATA_MAIN_CONFIG_T__SIZE ||
                len_conf > DATA_MAIN_CONFIG_T__SIZE - ofs_conf ||
                len_conf > (PACKET_MAX_PL_LEN - 10)) {
			break;
			}

			ind = 0;
			modCommandsSendBuffer[ind++] = packet_id;
			modCommandsSendBuffer[ind++] = conf_ind;
			libBufferAppend_int32(modCommandsSendBuffer, DATA_MAIN_CONFIG_T__SIZE, &ind);
			libBufferAppend_int32(modCommandsSendBuffer, ofs_conf, &ind);
			memcpy(modCommandsSendBuffer + ind, data_main_config_t_ + ofs_conf, len_conf);
			ind += len_conf;
			modCommandsSendPacket(modCommandsSendBuffer, ind);
			

		} break;
		default:
			break;
	}
	
	if(modDelayTick1ms(&modCommandsBootloaderDelayTick,1000) && jumpBootloaderTrue)
		modFlashJumpToBootloader();
}

void modCommandsPrintf(const char* format, ...) {
	va_list arg;
	va_start (arg, format);
	int len;
	static char print_buffer[255];

	print_buffer[0] = COMM_PRINT;
	len = vsnprintf(print_buffer+1, 254, format, arg);
	va_end (arg);

	if(len > 0) {
		modCommandsSendPacket((unsigned char*)print_buffer, (len<254)? len+1: 255);
	}
}

bool modCommandsVESCToEBMS(main_config_t *conf) {
    if (!conf || conf->cell_num <= 0 || conf->cell_num > 180 ||
        conf->temp_num < 0 || conf->temp_num > 9) return false;
    modConfigGeneralConfigStructTypedef next = modCommandsHasPendingConfig ?
        modCommandsPendingConfig : *modCommandsGeneralConfig;
    modConfigGeneralConfigStructTypedef *target = &next;
	
	target->CANID	= conf->controller_id;
	target->canBusSpeed = conf->can_baud_rate;
	target->noOfCellsSeries = conf->cell_num;
	target->noOfTempSensorPerModule = conf->temp_num;
	target->cellBalanceStart = conf->balance_start_voltage;
	target->cellBalanceDifferenceThreshold = conf->balance_difference_threshold;
	target->cellSoftOverVoltage = conf->soft_overvoltage;
	target->cellLCSoftUnderVoltage = conf->soft_undervoltage;
	target->cellHardOverVoltage = conf->hard_overvoltage;
	target->cellHardUnderVoltage = conf->hard_undervoltage;
	target->allowedTempBattChargingMax = conf->t_charge_max;
	target->allowedTempBattDischargingMax = conf->t_discharge_max;
	target->notUsedCurrentThreshold = conf->not_used_current_threshold;
	target->notUsedTimeout = conf->not_used_timeout;

    if (!modConfigValidate(&next)) return false;
    if (!modConfigStoreCandidate(&next)) return false;
    modCommandsPendingConfig = next;
    modCommandsHasPendingConfig = true;
    modCommandsSetCommissioningMode(true);
    return true;
}

void modCommandsEBMSToVESC(main_config_t *conf) {
    modCommandsConfigToVESC(conf, modCommandsHasPendingConfig ?
        &modCommandsPendingConfig : modCommandsGeneralConfig);
}

static void modCommandsConfigToVESC(main_config_t *conf,
    const modConfigGeneralConfigStructTypedef *config) {
	conf->controller_id = config->CANID;
	conf->can_baud_rate = config->canBusSpeed;
	conf->cell_num = config->noOfCellsSeries;
	conf->temp_num = config->noOfTempSensorPerModule;
	conf->balance_start_voltage = config->cellBalanceStart;
	conf->balance_difference_threshold = config->cellBalanceDifferenceThreshold;
	conf->soft_overvoltage = config->cellSoftOverVoltage;
	conf->soft_undervoltage = config->cellLCSoftUnderVoltage;
	conf->hard_overvoltage = config->cellHardOverVoltage;
	conf->hard_undervoltage = config->cellHardUnderVoltage;
	conf->t_charge_max = config->allowedTempBattChargingMax;
	conf->t_discharge_max = config->allowedTempBattDischargingMax;
	conf->not_used_current_threshold = config->notUsedCurrentThreshold;
	conf->not_used_timeout = config->notUsedTimeout;
}

void modCommandsJumpToMainApplication(void) {
	NVIC_SystemReset();
}
