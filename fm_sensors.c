/*
* fm_sensors.c
*
*  Created on: 5 Jan 2025
*      Author: jdutra
*/

#include "si91x_device.h"
#include "sl_si91x_driver_gpio.h"
#include "rsi_debug.h"
#include "fm_sensors.h"
#include "fm_sensor_types.h"
#include "sl_si91x_adc.h"
#include "sl_si91x_adc_common_config.h"
#include "sl_adc_instances.h"
#include "sl_si91x_clock_manager.h"
#include "rsi_rom_clks.h"
#include "thermistor_lookup.h"
#include "interpolation_search.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h> // For abs()
#include <limits.h>

/****** DEFINES ******/
#define PS4_SOC_FREQ          180000000  /*<! PLL out clock 180MHz            */
#define SOC_PLL_REF_FREQUENCY 40000000   /*<! PLL input REFERENCE clock 40MHZ */
#define DVISION_FACTOR        0          // Division factor
#define CHANNEL_SAMPLE_LENGTH 1     // Number of ADC sample collect for operation
#define ADC_MAX_OP_VALUE      4095       // Maximum unsigned 12-bit ADC value
#define ADC_MAX_OP_VALUE_SIGNED 2047     // Maximum signed 12-bit ADC magnitude
#define ADC_DATA_CLEAR        0xF7FF     // Clear the data if 12th bit is enabled (1111 0111 1111 1111)
#define VREF_VALUE            3.3        // reference voltage

#define ADC_PING_BUFFER_1     0x000A0000 // Start address of Ping for channel_0
#define ADC_PING_BUFFER_2     0x000B0000 // Start address of Ping for channel_1
#define ADC_PING_BUFFER_3     0x000C0000 // Start address of Ping for channel_2
#define ADC_PING_BUFFER_4     0x000D0000 // Start address of Ping for channel_3
#define ADC_PING_BUFFER_5     0x000E0000 // Start address of Ping for channel_4
#define ADC_PING_BUFFER_6     0x000F0000 // Start address of Ping for channel_5
#define ADC_PING_BUFFER_7     0x00100000 // Start address of Ping for channel_6

/******* LOCAL VARIABLES   *********/
static float vref_value = (float)VREF_VALUE;
static int16_t adc_output[NUMBER_OF_CHANNEL][CHANNEL_SAMPLE_LENGTH];
static boolean_t adc_sample_ready = false;

static fm_thermistor_handle thermistor_ctx;
static fm_pressure_handle pressure_ctx;
static fm_current_handle current_ctx;
static sensors_t _sensors; // struct representing the latest sensor values, updated by fm_read_all_sensors and read by fm_sensors_get_latest

static void callback_event(uint8_t event_channel, uint8_t event);

// The Silabs ADC initialization path requires an event callback to be
// registered. When the application uses synchronous read operations and
// does not rely on ADC event callbacks, this stub preserves the required
// callback registration without performing any action.
static void callback_event(uint8_t event_channel, uint8_t event)
{
  (void)event_channel;
  (void)event;
}

// Function to find the nearest value in the second column and return the corresponding first column value
int find_nearest_value(const float arr[], int size, float target) {
  int i = 1;
  if(0 != arr){
      for(; i < size; i++){
          if (target < arr[i]){
              return i;
          }
      }
  }
  return i;
}

float linear_interpolate(float R, float x0, float x1, float y0, float y1)
{
  float result = 0;
  if(x1 > x0) {
      float m = (y1 - y0)/(x1 - x0);
      return (R-x0)*m + y0;
  }
  return result;
}

float voltage_to_pressure(float v)
{
  const float R1 = 6.5f; // kOhms
  const float R2 = 10.0f;  // kOhms
  const float R_divider = R2/(R1+R2);

  const float v_max = pressure_ctx->voltage_max_v * R_divider;
  const float v_min = pressure_ctx->voltage_min_v* R_divider;
  const float y1 = pressure_ctx->pressure_max_mpa;
  const float y0 = pressure_ctx->pressure_min_mpa;
  const float x1 = pressure_ctx->voltage_max_v;
  const float x0 = pressure_ctx->voltage_min_v;
  const float transducer_factor = (y1 - y0)/(x1 - x0);
  const float resistor_divider_factor = (x1 - x0)/(v_max - v_min) ;

  float pressure = 0;

  if(v_max > v_min){
      pressure = v * transducer_factor * resistor_divider_factor;
      pressure = pressure*1000.0f; // convert to MPa to kPa
  }

  return pressure;
}

float voltage_to_current(float v)
{

  const float y1 = current_ctx->current_max_A;
  const float y0 = current_ctx->current_min_A;
  const float x1 = current_ctx->voltage_max_v;
  const float x0 = current_ctx->voltage_min_v;

  float current = 0;

  if(x1 > x0){

      current = v * ( (y1-y0)/(x1-x0) );
  }

  return current;
}

float voltage_to_temperature(float v)
{
//  float B = (float)thermistor_ctx->beta;       // Thermistor Beta value
//  float r_25 = thermistor_ctx->r_25;           // Thermistor resistance at 25 deg C
  float R_s = thermistor_ctx->r_s;             // Series resistor, here is 180 kOhm
  float V_ref = 3.300f;              // 3.3V reference
//  DEBUGOUT("v=%f, B=%f, r_25=%f, R_s=%f, V_ref=%f\n",v, B,r_25, R_s,V_ref);

  /* Convert voltage to resistance */
  float R_th = (v * R_s)/(V_ref - v);
  //float R_ntc = r_25 * exp(B*( (1/R_th) - (1/(170.15 + 25)) ) );
//  float temperature = 1 / ( (log(R_th/r_25)*(1/B) ) + (1/(170.15+25)));

   int idx = find_nearest_value(thermistor_lookup_R, NUM_LOOKUP_ENRIES, R_th);
   float x1 = thermistor_lookup_R[idx];
   float x0 = thermistor_lookup_R[idx - 1];
   float y1 = thermistor_lookup_T[idx];
   float y0 = thermistor_lookup_T[idx -1];
  float result = linear_interpolate(R_th, x0, x1, y0, y1);

  return result;
}

void fm_sensors_close()
{
  sl_si91x_adc_stop(sl_adc_config);
  sl_si91x_adc_deinit(sl_adc_config);
  if(0 != thermistor_ctx)
    free(thermistor_ctx);
  if(0 != pressure_ctx)
    free(pressure_ctx);
  if(0 != current_ctx)
    free(current_ctx);
}

void fm_sensors_init()
{
  thermistor_ctx = malloc(sizeof(struct fm_thermistor));
  pressure_ctx = malloc(sizeof(struct fm_pressure));
  current_ctx = malloc(sizeof(struct fm_pressure));

  /* All values taken from document RT103F3435BFA0-401 */
  thermistor_ctx->beta = 3435;  // kOhms
  thermistor_ctx->r_25 = 10;    // kOhms
  thermistor_ctx->r_s = 180;    // kOhms

  pressure_ctx->pressure_max_mpa = 4.0f; // Megapascals
  pressure_ctx->pressure_min_mpa = 0.1f; // Megapascals
  pressure_ctx->voltage_max_v = 5.0f;
  pressure_ctx->voltage_min_v = 0.0f;

  sl_adc_version_t version;
  sl_status_t status;

  DEBUGOUT("Clock configuration is successful \n");

  for (int i = 0; i < NUMBER_OF_CHANNEL; i ++) {
    sl_adc_channel_config.rx_buf[i] =
      adc_output[i]; // Use a distinct receive buffer for each ADC channel.
    // sl_adc_channel_config.chnl_ping_address[i] = ping_pong_addr_list[i]; // ADC ping address 
    // sl_adc_channel_config.chnl_pong_address[i] = ping_pong_addr_list[i]
    //   + (sl_adc_channel_config.num_of_samples[i]); // ADC pong address
  }

  do {
    // Version information of ADC driver
    version = sl_si91x_adc_get_version();
    DEBUGOUT("ADC version is fetched successfully \n");
    DEBUGOUT("API version is %d.%d.%d\n", version.release, version.major, version.minor);

    status = sl_si91x_adc_init(sl_adc_channel_config, sl_adc_config, vref_value);
    /* Due to calling trim_efuse API on ADC init in driver it will change the clock frequency,
        if we are not initialize the debug again it will print the garbage data in console output. */
    DEBUGINIT();
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_adc_init: Error Code : %lu \n", status);
      break;
    }
    DEBUGOUT("ADC Initialization Success\n");

    status = sl_si91x_adc_set_channel_configuration(sl_adc_channel_config, sl_adc_config);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_adc_channel_set_configuration: Error Code : %lu \n", status);
      break; 
    }
    DEBUGOUT("ADC Channel Configuration Successfully \n");
    // Register user callback function
    status = sl_si91x_adc_register_event_callback(callback_event);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_adc_register_event_callback: Error Code : %lu \n", status);
      break;
    }
    DEBUGOUT("ADC user event callback registered successfully \n");
    status = sl_si91x_adc_start(sl_adc_config);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_adc_start: Error Code : %lu \n", status);
      break;
    }

    for(int i = 0; i < NUMBER_OF_CHANNEL; i++){
      sl_si91x_adc_disable_ping_pong(i);
    }
    DEBUGOUT("Sensors initialised\n");
  } while (false);
}

void fm_read_all_sensors(void)
{
  sl_status_t status;
  float vout[NUMBER_OF_CHANNEL]              = {0.0f};
  uint16_t adc_output_static = 0;

  // here we get the 12-bit value of ADC output in equivalent voltage.
  sl_adc_channel_config.channel = SL_ADC_CHANNEL_1;
  for(int i = 0; i  < NUMBER_OF_CHANNEL; i++){
    // if (adc_sample_ready == true) {
      adc_sample_ready = false;
      int channel_read = sl_adc_channel_config.channel;
      status = sl_si91x_adc_read_data_static_v2(&sl_adc_channel_config, sl_adc_config, &adc_output_static);
      if (status != SL_STATUS_OK) {
        DEBUGOUT("sl_si91x_adc_read_data: Error Code : %lu \n", status);
      }
      adc_output[channel_read][0] = (int16_t)adc_output_static;
      if (adc_output[channel_read][0] & SIGN_BIT) {
        adc_output[channel_read][0] = (int16_t)(adc_output[channel_read][0] & (ADC_DATA_CLEAR));
      } else { // set the MSb bit.
        adc_output[channel_read][0] = adc_output[channel_read][0] | SIGN_BIT;
      }
      vout[channel_read] = (((float)adc_output[channel_read][0] / (float)ADC_MAX_OP_VALUE) * vref_value);

      // DEBUGOUT("ADC channel_%d :%0.2fV \n", channel_read, (float)vout[channel_read]);
      osDelay(3);
  }
  _sensors.thermistor_1 = vout[0]; 
  _sensors.thermistor_3 = vout[1]; 
  _sensors.thermistor_4 = vout[2]; 
  _sensors.pressure_1 = vout[3]; 
  _sensors.thermistor_2 = vout[4]; 
  _sensors.power = vout[5]; 
  _sensors.pressure_2 = vout[6]; 
}

void fm_sensors_get_latest(sensors_t* res)
{
  // TODO: put memory locks here
  *res = _sensors;
}

void get_sensor_name_string(ESensorType_t sensor, char * buffer)
{
  if(buffer != 0)
    {
      switch(sensor){
        case THERMISTOR_1: sprintf(buffer, "Themristor 1"); break;
        case THERMISTOR_2: sprintf(buffer, "Thermistor 2"); break;
        case THERMISTOR_3: sprintf(buffer, "Thermistor 3"); break;
        case THERMISTOR_4: sprintf(buffer, "Thermistor 4"); break;
        case PRESSURE_1: sprintf(buffer, "Pressure 1"); break;
        case PRESSURE_2: sprintf(buffer, "Pressure 2"); break;
        case POWER: sprintf(buffer, "Power"); break;
        case RESET_WIFI: sprintf(buffer, "Button"); break;
        case NO_SENSOR: sprintf(buffer, "No Sensor"); break;
        default: sprintf(buffer, "UNKNOWN");
      }
    }
}