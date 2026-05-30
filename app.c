/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include "rsi_debug.h"
#include "fm_sensors.h"
#include "fm_comms.h"
#include "app_ap_sel.h"
#include "ota_update.h"
#include "interpolation_search.h"
#include "sl_wifi_callback_framework.h"
#include "sl_si91x_hal_soc_soft_reset.h"

#include "sl_si91x_driver_gpio.h"
#include "sl_gpio_board.h"

#include "sl_si91x_rgb_led.h"
#include "sl_si91x_rgb_led_instances.h"

#include <stdio.h>
#include "cmsis_os2.h"
#include "sl_constants.h"
#include "string.h"

#include "nvm3_default.h"
#include <stdatomic.h>

static const char device_id[] = "A000001";

enum {
  LED_RED,
  LED_RED_FLASH,
  LED_CYAN,
  LED_CYAN_FLASH,
  LED_ORANGE,
  LED_ORANGE_FLASH,
  LED_PURPLE,
  LED_PURPLE_FLASH,
  LED_GREEN,
  LED_GREEN_FLASH,
  LED_YELLOW,
  LED_YELLOW_FLASH,
  LED_LAST
};

osThreadId_t led_thread_id;
const osThreadAttr_t led_thread_attributes =
{ .name = "led_thread", .attr_bits = 0, .cb_mem = 0, .cb_size = 0, .stack_mem = 0,
    .stack_size = 512, .priority = osPriorityLow, .tz_module = 0, .reserved =
        0, };

osThreadId_t button_thread_id;
const osThreadAttr_t button_thread_attributes =
  { .name = "button_thread", .attr_bits = 0, .cb_mem = 0, .cb_size = 0, .stack_mem = 0,
      .stack_size = 1024*4, .priority = osPriorityNormal, .tz_module = 0, .reserved =
          0, };

osThreadId_t main_thread_id;
const osThreadAttr_t main_thread_attr =
  {   .name = "main_thread",
      .attr_bits = 0,
      .cb_mem = 0,
      .cb_size = 0,
      .stack_mem = 0,
      .stack_size = 1024*8,
      .priority = osPriorityNormal,
      .tz_module = 0,
      .reserved = 0,};

osMessageQueueId_t queue_id;
osMessageQueueAttr_t queue_attr =
  { .name = "sensor_data"};

osMessageQueueId_t queue_led_id;
osMessageQueueAttr_t queue_led_attr =
  { .name = "led_cmd"};

static sl_si91x_gpio_pin_config_t sl_gpio_pin_config_49 = { { SL_SI91X_GPIO_49_PORT, SL_SI91X_GPIO_49_PIN }, GPIO_INPUT};



// static sl_si91x_gpio_pin_config_t sl_gpio_pin_config_30 = { { SL_SI91X_GPIO_30_PORT, SL_SI91X_GPIO_30_PIN }, GPIO_INPUT};
//static sl_rgb_led_t led;



void
led_thread (void *argument);
void
button_thread (void *argument);
void
fridge_monitor (void *argument);

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void
app_init ()
{

  queue_id = osMessageQueueNew (20, sizeof(fm_sensor_message_t), &queue_attr);

  queue_led_id = osMessageQueueNew (10, sizeof(uint8_t), &queue_led_attr);

  sl_gpio_driver_init();

  sl_gpio_set_configuration(sl_gpio_pin_config_49);

  led_thread_id = osThreadNew ((osThreadFunc_t) led_thread, NULL,
                                &led_thread_attributes);

  button_thread_id = osThreadNew ((osThreadFunc_t) button_thread, NULL,
                                &button_thread_attributes);

  main_thread_id = osThreadNew ((osThreadFunc_t) fridge_monitor, NULL,
                                  &main_thread_attr);
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/

void
button_thread (void *argument)
{
  UNUSED_PARAMETER(argument);
  const uint8_t BUTTON_PRESSED = 0U;
  const uint8_t BUTTON_RELEASED = 1U;
  uint32_t decisecond_on_counter = 0;
  uint32_t decisecond_off_counter = 0;
  osDelay(50);
  DEBUGOUT("Started button Thread\r\n");
  uint8_t btn_state = 1U;
  uint8_t num_consecutive_press = 0U;
  uint8_t last_state = 1U;

  const uint16_t SHORT_PRESS_DURATION_THRESHOLD = 20; // 2 seconds on time
  const uint16_t SHORT_RELEASE_DURATION_THRESHOLD = 20; //2 seconds off time
  const uint16_t DECISECOND_DELAY_VAL = 100;
  const uint16_t TIMEOUT_ACCESS_POINT_RESET = 30;

  for(;;){
    btn_state = sl_gpio_get_pin_input(SL_SI91X_GPIO_49_PORT,SL_SI91X_GPIO_49_PIN);
    if(BUTTON_PRESSED == btn_state){ //pressed
//        DEBUGOUT("Button pressed\n");
        decisecond_on_counter++;

        if(BUTTON_RELEASED == last_state){
            if(SHORT_RELEASE_DURATION_THRESHOLD < decisecond_off_counter)
              num_consecutive_press = 0;

            decisecond_off_counter = 0;
        }
        last_state = btn_state;
    }
    if(BUTTON_RELEASED == btn_state){
//        DEBUGOUT("Button released\n");
        decisecond_off_counter++;

        if(BUTTON_PRESSED == last_state){
          if(SHORT_PRESS_DURATION_THRESHOLD > decisecond_on_counter){
              num_consecutive_press++;
              DEBUGOUT("Presses: %d\n",num_consecutive_press);
          }
          decisecond_on_counter = 0;
        }
        last_state = btn_state;
    }

    if(num_consecutive_press > 4){
        DEBUGOUT("Rquesting OTA Update...\n");
        ESensorType_t msg = OTA_UPDATE;
        osMessageQueuePut (queue_id, &msg, 0, osWaitForever);
        num_consecutive_press = 0;
        decisecond_on_counter = 0;
    }
    if(TIMEOUT_ACCESS_POINT_RESET < decisecond_on_counter){
        DEBUGOUT("Requesting WiFi reset... \n");
        ESensorType_t msg = RESET_WIFI;
        osMessageQueuePut (queue_id, &msg, 0, osWaitForever);
        decisecond_on_counter = 0;
    }


    osDelay(DECISECOND_DELAY_VAL);
  }
}

void
led_thread(void *argument)
{

  sl_si91x_simple_rgb_led_init(&led_led0);
  sl_si91x_simple_rgb_led_on(&led_led0);

  uint8_t led_cmd = LED_LAST;
  UNUSED_PARAMETER(argument);
  DEBUGOUT("Started LED Thread\r\n");
  for(;;) {
      osMessageQueueGet(queue_led_id, &led_cmd, 0, 0);
      int colour = 0xFF66FF; // default pink
      int delay = 0;
      switch (led_cmd) {
        case LED_RED: colour = 0xFF3333;delay = 0;break;
        case LED_RED_FLASH: colour = 0xFF3333;delay = 100;break;
        case LED_CYAN: colour = 0x33FFFF;delay = 0;break;
        case LED_CYAN_FLASH: colour = 0x33FFFF;delay = 100;break;
        case LED_ORANGE: colour = 0xFF8000;delay = 0;break;
        case LED_ORANGE_FLASH: colour = 0xFF8000;delay = 100;break;
        case LED_PURPLE: colour = 0xB266FF;delay = 0;break;
        case LED_PURPLE_FLASH: colour = 0xB266FF;delay = 100;break;
        case LED_GREEN: colour = 0x00CC00;delay = 0;break;
        case LED_GREEN_FLASH: colour = 0x00CC00;delay = 1000;break;
        case LED_YELLOW: colour = 0xE5EB34;delay = 0;break;
        case LED_YELLOW_FLASH: colour = 0xE5EB34;delay = 200;break;
      }

          sl_si91x_simple_rgb_led_set_colour(&led_led0, colour);
          sl_si91x_simple_rgb_led_on(&led_led0);
      if(delay > 0) {
          osDelay(delay);
          sl_si91x_simple_rgb_led_off(&led_led0);
          osDelay(delay);
      }
  }
}

int publish_sensor_data(ESensorType_t sensor_type, float value)
{
  char buf_data[1023];
  char buf_topic[60];
  // For troubleshooing purposes I have removed all the volatgae conversions.
  // Add back in by replacing eachine line like:
  //printf(buf_data, "%0.2f", (value));
  // with:
  //printf(buf_data, "%0.2f", voltage_to_temperature(value));
  switch (sensor_type)
  {
    case THERMISTOR_1:
      sprintf(buf_data, "%0.2f", value);
      strcpy(buf_topic,"temperature1");
      break;
    case THERMISTOR_2:
      sprintf(buf_data, "%0.2f", (value));
      strcpy(buf_topic, "temperature2");
      break;
    case THERMISTOR_3:
      sprintf(buf_data, "%0.2f", (value));
      strcpy(buf_topic, "temperature3");
      break;
    case THERMISTOR_4:
      sprintf(buf_data, "%0.2f", (value));
      strcpy(buf_topic, "temperature4");
      break;
    case PRESSURE_1:
      strcpy(buf_topic, "pressure1");
      break;
    case PRESSURE_2:
      sprintf(buf_data, "%0.2f", (value));
      strcat(buf_topic, "pressure2");
      break;
    case POWER:
      sprintf(buf_data, "%0.2f", (value));
      strcpy(buf_topic, "power");
      break;
    default:
      break;
  }
  char buf[100] = {0};
  sprintf(buf,"%s/%s", device_id, buf_topic);
  DEBUGOUT("%s: %s\r\n",buf, buf_data);

  fm_comms_status status = fm_comms_publish_data (
      buf_data,
      strlen (buf_data),
      buf,
      strlen (buf));

  if(status != FMCOMMS_SUCCESS){
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

typedef enum  {
  INITIALISING,
  PROVISIONING,
  OPERATING,
  STOPPING,
  RECONNECTING,
  CONNECTING,
  MQTT,
  NO_SETUP,
  OTA_FIRMWARE_UPDATE,
  FM_ERROR,
}E_FM_State_t;

void printstate(E_FM_State_t const*const state){
  switch(*state) {
    case INITIALISING: printf("INITIALISING"); break;
    case PROVISIONING: printf("PROVISIONING"); break;
    case OPERATING: printf("OPERATING"); break;
    case STOPPING: printf("STOPPING"); break;
    case RECONNECTING: printf("RECONNECTING"); break;
    case CONNECTING: printf("CONNECTING"); break;
    case MQTT: printf("MQTT"); break;
    case NO_SETUP: printf("NO_SETUP"); break;
    case FM_ERROR: printf("FM_ERROR"); break;
    default:break;

  }
}

void
fridge_monitor (void *argument)
{
  UNUSED_PARAMETER(argument);
  DEBUGOUT("Started Main Thread\r\n");

  E_FM_State_t state = INITIALISING;
  E_FM_State_t last_state = state;
  uint8_t reconnect_countdown = 10;

  uint8_t led_cmd = LED_LAST;
  for(;;){
      if(state != FM_ERROR)
        last_state = state; // preserve last state for troubleshooting
      ESensorType_t msg;
      osMessageQueueGet(queue_id, &msg, 0, 0);
      if(msg == RESET_WIFI){
          state = PROVISIONING;
      }

      if(msg == OTA_UPDATE){ // TODO: make new type for these command messages
          state = OTA_FIRMWARE_UPDATE;
      }

      switch(state){
        case INITIALISING:{
          led_cmd = LED_ORANGE_FLASH;
          fm_comms_status status = FMCOMMS_NOT_INITIALISED;

          status = fm_comms_init();
          if(FMCOMMS_SUCCESS == status){
              state = CONNECTING;
          }else{
              state = FM_ERROR;
          }
          break;
        }
        case CONNECTING:{
          led_cmd = LED_ORANGE;
          fm_comms_status s = 0;
              s = fm_comms_connect();
          if(FMCOMMS_SUCCESS == s){
              state = MQTT;
          }else if(FMCOMMS_WIFI_NOT_FOUND == s){
              state = CONNECTING;
              osDelay(2000); // If WiFi didn't connect, wait 2sec and try again
          }else if(FMCOMMS_NOT_INITIALISED){
              state = INITIALISING;
          }else if(FMCOMMS_NVM_EMPTPY == s){
            state = NO_SETUP;
          } else {
              state = FM_ERROR;
          }
          break;
        }
        case MQTT:{
          led_cmd = LED_CYAN_FLASH;
          if(1U == fm_is_wifi_connected()){
              fm_comms_status s = fm_comms_mqtt_start();
              if(FMCOMMS_SUCCESS == s){
                  fm_sensors_init();
                  state = OPERATING;
              }else if(FMCOMMS_MQTT_ERROR){
                  state = MQTT;
                  osDelay(5000); // If MQTT failed, wait 2sec and try again
              } else {
                  state = FM_ERROR;
              }
          }else {
              state = CONNECTING;
          }
          break;
        }
        case OPERATING:
            led_cmd = LED_GREEN;
            // Ensure MQTT is connected. If MQTT fails WiFi will be checked
            if(1U != fm_is_mqtt_connected()){
                fm_sensors_close();
                state = MQTT;
            } else {
              float vout;
              ESensorType_t sensor = fm_sensors_read(&vout);
              if(NO_SENSOR != sensor){
                if(EXIT_SUCCESS != publish_sensor_data(sensor,vout)){
                    fm_comms_deint();
                    fm_sensors_close();
                    state = INITIALISING;
                } else {
                  state = OPERATING;
                }
              }
            }
          break;
        case PROVISIONING:
          led_cmd = LED_PURPLE_FLASH;
          osMessageQueuePut(queue_led_id, &led_cmd, 0, 0);
          fm_comms_deint();
          fm_sensors_close();
          fm_ap_sel_start();
          state = INITIALISING;
          break;
        case RECONNECTING:
          led_cmd = LED_GREEN_FLASH;
          if(reconnect_countdown-- > 0){
              DEBUGOUT("Attempting reconnection in %d...\n",reconnect_countdown);
              osDelay(500);
          }else{
            reconnect_countdown = 5;
            fm_comms_deint();
            state = INITIALISING;
          }
          break;
        case NO_SETUP:
          led_cmd = LED_RED_FLASH;
          printf("waiting for setup...\n");
          osDelay(1000);
          break;
        case OTA_FIRMWARE_UPDATE:
          {
            uint8_t s;
            led_cmd = LED_YELLOW;
            printf("starting OTA firmware update...\n");
            if(1U != fm_is_wifi_connected()){
                printf("Internet connection required for OTA update!\r\n");
                break;
            }
            //fm_comms_mqtt_stop();
            //DEBUGOUT("MQTT Disabled\n");
            osDelay(100);
            s = ota_update();
            if (s != 0U) {
                printf("\r\n Firmware update failed\r\n");
                break;
            } else {
                DEBUGOUT("\r\n Firmware update succeeded\r\n");
                //fm_comms_deint();
                DEBUGOUT("Waiting for update to finish...");
                osDelay(10000);
                printf("\r\nSoC Soft Reset initiated!\r\n");
                sl_si91x_soc_nvic_reset();
            }
            state = INITIALISING;

            state = FM_ERROR;
            break;
        case FM_ERROR:
          led_cmd = LED_RED;
          printf("STATE MACHINE ERROR. LAST STATE: ");
          printstate(&last_state);
          printf("\r\n");
          osDelay(5000);
          break;
        default:
          break;
      }
      osMessageQueuePut(queue_led_id, &led_cmd, 0, 0);
      osDelay(50);
      if(state != last_state){ // dont spam the console
        printstate(&last_state);printf("-->");printstate(&state);printf("\n");
      }
    }
  }
}



//void
//app_process_action (void)
//{
//  osDelay (200);
//}

