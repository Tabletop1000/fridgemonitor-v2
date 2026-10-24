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
#define CHANNEL_SAMPLE_LENGTH 100        // Number of ADC samples per channel per FIFO buffer, must match SL_ADC_CHANNEL_x_SAMPLE_LENGTH
#define ADC_MAX_OP_VALUE      4095       // Maximum unsigned 12-bit ADC value
#define ADC_MAX_OP_VALUE_SIGNED 2047     // Maximum signed 12-bit ADC magnitude
#define ADC_DATA_CLEAR        0xF7FF     // Clear the data if 12th bit is enabled (1111 0111 1111 1111)
#define VREF_VALUE            3.3        // reference voltage
#define ADC_READ_TIMEOUT_MS   500        // Max time to wait for a fresh FIFO buffer on every channel
#define ADC_READ_POLL_MS      5          // Poll interval while waiting for FIFO buffers
#define ADC_ALL_CHANNELS_MASK ((1UL << NUMBER_OF_CHANNEL) - 1)
#define ADC_BUFFER_PING       1          // RSI_ADC_ReadData ping_pong argument: read ping buffer
#define ADC_BUFFER_PONG       0          // RSI_ADC_ReadData ping_pong argument: read pong buffer

/******* LOCAL VARIABLES   *********/
static float vref_value = (float)VREF_VALUE;
static int16_t adc_output[NUMBER_OF_CHANNEL][CHANNEL_SAMPLE_LENGTH];

// ADC internal DMA ping/pong memory, one region per channel. The driver lays
// the ping buffer out at chnl_ping_address and the pong buffer directly after
// it (chnl_ping_address + 2 * num_of_samples bytes), so each channel needs
// room for 2 * CHANNEL_SAMPLE_LENGTH samples. Keeping this as a static buffer
// (instead of a fixed address as in the Silabs example) keeps the linker from
// placing other data on top of it.
static int16_t adc_ping_pong_mem[NUMBER_OF_CHANNEL][2 * CHANNEL_SAMPLE_LENGTH] __attribute__((aligned(4)));

// Set from the ADC IRQ: bit n is set once channel n has filled a ping or pong buffer.
static volatile uint32_t adc_buffer_ready_mask = 0;
// Which buffer each channel filled most recently (ADC_BUFFER_PING = ping, ADC_BUFFER_PONG = pong).
static volatile uint8_t adc_last_filled[NUMBER_OF_CHANNEL];
// Which buffer each channel fills next. The hardware always starts with ping.
static uint8_t adc_next_fill[NUMBER_OF_CHANNEL];

static fm_thermistor_handle thermistor_ctx;
static fm_pressure_handle pressure_ctx;
static fm_current_handle current_ctx;
static sensors_t _sensors; // struct representing the latest sensor values, updated by fm_read_all_sensors and read by fm_sensors_get_latest

static void callback_event(uint8_t event_channel, uint8_t event);

// Called from the ADC IRQ each time a channel fills its ping or pong buffer.
// The ADC keeps sampling in FIFO mode, alternating ping then pong.
static void callback_event(uint8_t event_channel, uint8_t event)
{
  if ((event == SL_INTERNAL_DMA) && (event_channel < NUMBER_OF_CHANNEL)) {
    adc_last_filled[event_channel] = adc_next_fill[event_channel];
    adc_next_fill[event_channel] =
      (adc_next_fill[event_channel] == ADC_BUFFER_PING) ? ADC_BUFFER_PONG : ADC_BUFFER_PING;
    adc_buffer_ready_mask |= (1UL << event_channel);
  }
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
  sl_status_t status = SL_STATUS_OK;

  DEBUGOUT("Clock configuration is successful \n");

  for (int i = 0; i < NUMBER_OF_CHANNEL; i ++) {
    sl_adc_channel_config.rx_buf[i] =
      adc_output[i]; // Use a distinct receive buffer for each ADC channel.
    sl_adc_channel_config.chnl_ping_address[i] = (uint32_t)adc_ping_pong_mem[i]; // ADC ping address
    sl_adc_channel_config.chnl_pong_address[i] = (uint32_t)adc_ping_pong_mem[i]
      + (sl_adc_channel_config.num_of_samples[i]); // ADC pong address
    adc_next_fill[i] = ADC_BUFFER_PING;
    adc_last_filled[i] = ADC_BUFFER_PING;
  }
  adc_buffer_ready_mask = 0;

  do {
    for (int i = 0; i < NUMBER_OF_CHANNEL; i++) {
      if (sl_adc_channel_config.num_of_samples[i] != CHANNEL_SAMPLE_LENGTH) {
        DEBUGOUT("ADC channel_%d sample length %d does not match CHANNEL_SAMPLE_LENGTH %d\n",
                 i, sl_adc_channel_config.num_of_samples[i], CHANNEL_SAMPLE_LENGTH);
        status = SL_STATUS_INVALID_CONFIGURATION;
      }
    }
    if (status == SL_STATUS_INVALID_CONFIGURATION) {
      break;
    }

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
    DEBUGOUT("Sensors initialised\n");
  } while (false);
}

void fm_read_all_sensors(void)
{
  float vout[NUMBER_OF_CHANNEL]              = {0.0f};
  float vout_rms[NUMBER_OF_CHANNEL]          = {0.0f};
  uint32_t waited_ms = 0;

  // Discard buffers completed before this call and wait until every channel
  // has filled a fresh ping or pong buffer of CHANNEL_SAMPLE_LENGTH samples.
  adc_buffer_ready_mask = 0;
  while (((adc_buffer_ready_mask & ADC_ALL_CHANNELS_MASK) != ADC_ALL_CHANNELS_MASK)
         && (waited_ms < ADC_READ_TIMEOUT_MS)) {
    osDelay(ADC_READ_POLL_MS);
    waited_ms += ADC_READ_POLL_MS;
  }
  uint32_t ready_mask = adc_buffer_ready_mask;

  for (uint8_t ch = 0; ch < NUMBER_OF_CHANNEL; ch++) {
    if (!(ready_mask & (1UL << ch))) {
      DEBUGOUT("ADC channel_%d: no FIFO data after %lu ms\n", ch, waited_ms);
      continue;
    }
    // Read the most recently completed buffer. The ADC keeps filling the other
    // one, so this buffer is stable for another CHANNEL_SAMPLE_LENGTH samples.
    // RSI_ADC_ReadData is used rather than sl_si91x_adc_read_data because the
    // latter tracks ping/pong per read rather than per completed buffer, and
    // only for channels 0-3.
    rsi_error_t error_status = RSI_ADC_ReadData(adc_output[ch],
                                                adc_last_filled[ch],
                                                ch,
                                                1, // apply gain and offset calibration
                                                sl_adc_channel_config.input_type[ch]);
    if (error_status != RSI_OK) {
      DEBUGOUT("RSI_ADC_ReadData: channel_%d Error Code : %d \n", ch, error_status);
      continue;
    }

    float vsum = 0.0f;
    float vsum_sqrs = 0.0f;
    for (uint32_t i = 0; i < CHANNEL_SAMPLE_LENGTH; i++) {
      /* In two's complement format, the MSb (11th bit) of the conversion result determines the polarity,
         when the MSb = '0', the result is positive, and when the MSb = '1', the result is negative */
      if (adc_output[ch][i] & SIGN_BIT) {
        adc_output[ch][i] = (int16_t)(adc_output[ch][i] & (ADC_DATA_CLEAR));
      } else { // set the MSb bit.
        adc_output[ch][i] = adc_output[ch][i] | SIGN_BIT;
      }
      float v = (((float)adc_output[ch][i] / (float)ADC_MAX_OP_VALUE) * vref_value);
      // For differential type it will give vout.
      if (sl_adc_channel_config.input_type[ch]) {
        v = v - (vref_value / 2);
      }
      vsum += v;
      vsum_sqrs += v*v;
    }
    vout[ch] = vsum / (float)CHANNEL_SAMPLE_LENGTH;
    vout_rms[ch] = sqrt(vsum_sqrs/(float)CHANNEL_SAMPLE_LENGTH);

    // DEBUGOUT("ADC channel_%d :%0.2fV \n", ch, (double)vout[ch]);
  }
  _sensors.thermistor_1 = vout[0]; 
  _sensors.thermistor_3 = vout[1]; 
  _sensors.thermistor_4 = vout[2]; 
  _sensors.pressure_1 = vout[3]; 
  _sensors.thermistor_2 = vout[4]; 
  _sensors.power = vout_rms[5]; 
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