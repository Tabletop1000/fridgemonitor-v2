/*
 * fm_sensor_types.h
 *
 *  Created on: 19 Feb 2025
 *      Author: jdutra
 */

#ifndef FM_SENSOR_TYPES_H_
#define FM_SENSOR_TYPES_H_

/*******************************************************************************
***************************  Defines / Macros  ********************************
******************************************************************************/
/* Core clock modification Macros */
#define PS4_SOC_FREQ          180000000  /*<! PLL out clock 180MHz            */
#define SOC_PLL_REF_FREQUENCY 40000000   /*<! PLL input REFERENCE clock 40MHZ */
#define DVISION_FACTOR        0          // Division factor
#define CHANNEL_SAMPLE_LENGTH 20     // Number of ADC sample collect for operation
#define ADC_MAX_OP_VALUE      4096       // Maximum output value get from adc data register
#define ADC_DATA_CLEAR        0xF7FF     // Clear the data if 12th bit is enabled
#define VREF_VALUE            3.3        // reference voltage
#define ADC_PING_BUFFER_1     0x00000A00 // Start address of Ping for channel_0
#define ADC_PING_BUFFER_2     0x00000B00 // Start address of Ping for channel_1
#define ADC_PING_BUFFER_3     0x00000C00 // Start address of Ping for channel_2
#define ADC_PING_BUFFER_4     0x00000D00 // Start address of Ping for channel_3
#define ADC_PING_BUFFER_5     0x00000E00 // Start address of Ping for channel_4
#define ADC_PING_BUFFER_6     0x00000F00 // Start address of Ping for channel_5
#define ADC_PING_BUFFER_7     0x00001000 // Start address of Ping for channel_6

#define SOC_PLL_CLK          ((uint32_t)(180000000)) // 180MHz default SoC PLL Clock as source to Processor
#define INTF_PLL_CLK         ((uint32_t)(180000000)) // 180MHz default Interface PLL Clock as source to all peripherals
#define QSPI_ODD_DIV_ENABLE  0                       // Odd division enable for QSPI clock
#define QSPI_SWALLO_ENABLE   0                       // Swallo enable for QSPI clock
#define QSPI_DIVISION_FACTOR 0                       // Division factor for QSPI clock

struct fm_thermistor{
  int beta;           // Beta value used for exponential function
  float r_25;        // Reisistance at 25 deg C
  float r_s;          // Series resistor
};
typedef struct fm_thermistor * fm_thermistor_handle;

struct fm_current{
  float current_max_A;
  float current_min_A;
  float voltage_max_v;
  float voltage_min_v;
};
typedef struct fm_current * fm_current_handle;

struct fm_pressure{
  float pressure_max_mpa;
  float pressure_min_mpa;
  float voltage_max_v;
  float voltage_min_v;
};
typedef struct fm_pressure * fm_pressure_handle;

#endif /* FM_SENSOR_TYPES_H_ */
