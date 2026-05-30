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
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

/*******************************************************************************
*************************** LOCAL VARIABLES   *******************************
******************************************************************************/
static float vref_value = (float)VREF_VALUE;
static int16_t adc_output[CHANNEL_SAMPLE_LENGTH];
static boolean_t ch_flags[NUMBER_OF_CHANNEL];

static fm_thermistor_handle thermistor_ctx;
static fm_pressure_handle pressure_ctx;
static fm_current_handle current_ctx;


/*******************************************************************************
**********************  Local Function prototypes   ***************************
******************************************************************************/
static void callback_event(uint8_t event_channel, uint8_t event);
static void default_clock_configuration(void);

// Function to configure clock on powerup
static void default_clock_configuration(void)
{
  // Core Clock runs at 180MHz SOC PLL Clock
  sl_si91x_clock_manager_m4_set_core_clk(M4_SOCPLLCLK, SOC_PLL_CLK);

  // All peripherals' source to be set to Interface PLL Clock
  // and it runs at 180MHz
  sl_si91x_clock_manager_set_pll_freq(INFT_PLL, INTF_PLL_CLK, PLL_REF_CLK_VAL_XTAL);

  // Configure QSPI clock as input source
  ROMAPI_M4SS_CLK_API->clk_qspi_clk_config(M4CLK,
                                           QSPI_INTFPLLCLK,
                                           QSPI_SWALLO_ENABLE,
                                           QSPI_ODD_DIV_ENABLE,
                                           QSPI_DIVISION_FACTOR);
}

/*******************************************************************************
* Callback event function
* It is responsible for the event which are triggered by ADC interface
* @param  event       : INTERNAL_DMA => Single channel data acquisition done.
*                       ADC_STATIC_MODE_CALLBACK => Static mode adc data
*                       acquisition done.
******************************************************************************/
static void callback_event(uint8_t event_channel, uint8_t event)
{
  if (event == SL_INTERNAL_DMA) {
    switch (event_channel) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
        ch_flags[event_channel] = true;
        break;

      default:
        break;
    }
  }
}


#include <stdio.h>
#include <stdlib.h> // For abs()

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

/*******************************************************************************
* Function will run continuously and will wait for trigger
******************************************************************************/

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

  // default clock configuration by application common for whole system
  default_clock_configuration();


  sl_adc_channel_config.rx_buf[0] =
    adc_output; /* In order for us to read the data from adc_output in the sample application, ADC will save the data in the rx buffer. */
  sl_adc_channel_config.chnl_ping_address[0] =
    ADC_PING_BUFFER_1; /* Starting address of ADC Ping buffer for channel 0 */
  sl_adc_channel_config.chnl_pong_address[0] =
    ADC_PING_BUFFER_1
    + (sl_adc_channel_config.num_of_samples[0]); /* Starting address of ADC Pong buffer for channel 0 */

  sl_adc_channel_config.rx_buf[1] =
    adc_output; /* In order for us to read the data from adc_output in the sample application, ADC will save the data in the rx buffer. */
  sl_adc_channel_config.chnl_ping_address[1] =
    ADC_PING_BUFFER_2; /* Starting address of ADC Ping buffer for channel 1 */
  sl_adc_channel_config.chnl_pong_address[1] =
    (ADC_PING_BUFFER_2
     + (sl_adc_channel_config.num_of_samples[1])); /* Starting address of ADC Pong buffer for channel 1 */

  sl_adc_channel_config.rx_buf[2] =
    adc_output; /* In order for us to read the data from adc_output in the sample application, ADC will save the data in the rx buffer. */
  sl_adc_channel_config.chnl_ping_address[2] =
    ADC_PING_BUFFER_3; /* Starting address of ADC Ping buffer for channel 2 */
  sl_adc_channel_config.chnl_pong_address[2] =
    (ADC_PING_BUFFER_3
     + (sl_adc_channel_config.num_of_samples[2])); /* Starting address of ADC Pong buffer for channel 2 */

  sl_adc_channel_config.rx_buf[3] =
    adc_output; /* In order for us to read the data from adc_output in the sample application, ADC will save the data in the rx buffer. */
  sl_adc_channel_config.chnl_ping_address[3] =
    ADC_PING_BUFFER_4; /* Starting address of ADC Ping buffer for channel 3 */
  sl_adc_channel_config.chnl_pong_address[3] =
    (ADC_PING_BUFFER_4
     + (sl_adc_channel_config.num_of_samples[3])); /* Starting address of ADC Pong buffer for channel 3 */

  sl_adc_channel_config.rx_buf[4] =
    adc_output; /* In order for us to read the data from adc_output in the sample application, ADC will save the data in the rx buffer. */
  sl_adc_channel_config.chnl_ping_address[4] =
    ADC_PING_BUFFER_5; /* Starting address of ADC Ping buffer for channel 3 */
  sl_adc_channel_config.chnl_pong_address[4] =
    (ADC_PING_BUFFER_5
     + (sl_adc_channel_config.num_of_samples[4])); /* Starting address of ADC Pong buffer for channel 3 */

  sl_adc_channel_config.rx_buf[5] =
    adc_output; /* In order for us to read the data from adc_output in the sample application, ADC will save the data in the rx buffer. */
  sl_adc_channel_config.chnl_ping_address[5] =
    ADC_PING_BUFFER_6; /* Starting address of ADC Ping buffer for channel 3 */
  sl_adc_channel_config.chnl_pong_address[5] =
    (ADC_PING_BUFFER_6
     + (sl_adc_channel_config.num_of_samples[5])); /* Starting address of ADC Pong buffer for channel 3 */

  sl_adc_channel_config.rx_buf[6] =
    adc_output; /* In order for us to read the data from adc_output in the sample application, ADC will save the data in the rx buffer. */
  sl_adc_channel_config.chnl_ping_address[6] =
    ADC_PING_BUFFER_7; /* Starting address of ADC Ping buffer for channel 3 */
  sl_adc_channel_config.chnl_pong_address[6] =
    (ADC_PING_BUFFER_7
     + (sl_adc_channel_config.num_of_samples[6])); /* Starting address of ADC Pong buffer for channel 3 */

  // Disable all ping-pong nonsense
  for(int i = 0; i < NUMBER_OF_CHANNEL; i ++){
      sl_si91x_adc_disable_ping_pong(i);
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
    DEBUGOUT("ADC initialised\n");
  } while (false);
}


ESensorType_t fm_sensors_read(float *const data)
{
  sl_status_t status;
  uint32_t sample_length;
  static uint8_t chnl_num = 0;
  ESensorType_t ret = NO_SENSOR;
  float vout              = 0.0f;
  int32_t avg_adc_output  = 0;

  // here we get the 12-bit value of ADC output in equivalent voltage.
  if (ch_flags[chnl_num] == true) {
    ret = (ESensorType_t)chnl_num;
    ch_flags[chnl_num] = false;
    status             = sl_si91x_adc_read_data(sl_adc_channel_config, chnl_num);
    if (status != SL_STATUS_OK) {
      DEBUGOUT("sl_si91x_adc_read_data: Error Code : %lu \n", status);
    }
    for (sample_length = 0; sample_length < sl_adc_channel_config.num_of_samples[chnl_num]; sample_length++) {
      /* In two’s complement format, the MSb (11th bit) of the conversion result determines the polarity,
       when the MSb = ‘0’, the result is positive, and when the MSb = ‘1’, the result is negative*/
      if (adc_output[sample_length] & SIGN_BIT) {
        // Full-scale would be represented by a hexadecimal value, full-scale range of ADC result values in two’s complement.
        adc_output[sample_length] &= (int16_t)(ADC_DATA_CLEAR);
      } else { // set the MSb bit.
        adc_output[sample_length] |= SIGN_BIT;
      }
      avg_adc_output += adc_output[sample_length];
    }
    avg_adc_output /= sample_length;
    vout = (((float)avg_adc_output / (float)ADC_MAX_OP_VALUE) * vref_value);
    //For differential type it will give vout.
    if (sl_adc_channel_config.input_type[chnl_num]) {
      vout = vout - (vref_value / 2);
    }
    *data = vout;

    //DEBUGOUT("ADC channel_%d[%ld] :%0.2fV \n", chnl_num, sample_length, (float)vout);
    if (++chnl_num >= NUMBER_OF_CHANNEL) {
      chnl_num = 0;
      //DEBUGOUT("\n\n");
    }
  }
  return ret;
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
