#include "stm32f3xx_hal.h"
#include "stdbool.h"
#include "math.h"
#include "generalDefines.h"

#define NoOfADCPorts 2
#define NoOfAverages 10

/* ----------------------------------------------------------------------------
 * External LEM DHAB current sensor, wired DIRECTLY to the STM32 internal ADC.
 *
 * HARDWARE TODO (verify before trusting current readings):
 *   1. Pick a free ADC1 input pin for the DHAB output(s) and set DHAB_ADC_CHANNEL_CHx
 *      below. The pin must ALSO be added as an analog GPIO in driverHWADCPorts[]
 *      (driverHWADC.c) or the reading will be garbage. Nearly all GPIOA pins are
 *      already used on this board - confirm the chosen pin is truly free.
 *   2. The STM32 ADC full-scale is 3.3V. A LEM DHAB fed from 5V swings around a
 *      2.5V mid-point and can exceed 3.3V - use a divider (or a 3.3V-referenced
 *      front end) and update DHAB_ZERO_V / DHAB_SENS_* to the post-divider values.
 *   3. Confirm the DHAB variant (single-range vs dual-range S/161) and its
 *      sensitivity from the datasheet.
 * Values below are placeholders carried over from the S/161 dual-range design.
 * -------------------------------------------------------------------------- */
#define DHAB_ADC_CHANNEL_CH1   ADC_CHANNEL_2   // TODO: set to the pin the DHAB CH1 (fine range) is wired to
#define DHAB_ADC_CH2_ENABLED   0               // Set to 1 and define DHAB_ADC_CHANNEL_CH2 for dual-range S/161
#define DHAB_ADC_CHANNEL_CH2   ADC_CHANNEL_2   // TODO: set to the pin the DHAB CH2 (coarse range) is wired to
#define DHAB_VREF_V            3.3f            // ADC reference voltage
#define DHAB_ZERO_V            2.5f            // Sensor output at 0A (post-divider); UPDATE for your front end
#define DHAB_SENS_CH1_VperA    0.020f          // CH1 sensitivity, V per A (S/161 fine, +/-100A)
#define DHAB_SENS_CH2_VperA    0.003125f       // CH2 sensitivity, V per A (S/161 coarse, +/-800A)
#define DHAB_SWITCH_A          80.0f           // Use CH1 below this magnitude, else CH2

typedef struct {
	GPIO_TypeDef* Port;
	uint32_t ClkRegister;
	uint32_t Pin;
	uint32_t Mode;
	uint32_t Pull;
	uint32_t Alternate;
} driverHWADCPortStruct;

void driverHWADCInit(void);
void driverHWADCSetInputChannel(ADC_HandleTypeDef* hadc, uint32_t inputChannel);
bool driverHWADCGetLoadVoltage(float *loCurrentLoadVoltage, float offset, float scalar);
bool driverHWADCGetChargerVoltage(float *ChargerVoltage, float offset, float scalar);
bool driverHWADCGetNTCValue(float *ntcValue, uint32_t ntcNominal, uint32_t ntcSeriesResistance, uint16_t ntcBetaFactor, float ntcNominalTemp);
bool driverHWADCGetDHABCurrent(float *packCurrent);

