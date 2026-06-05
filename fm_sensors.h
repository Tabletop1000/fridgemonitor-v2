/*
 * fm_sensors.h
 *
 *  Created on: 5 Jan 2025
 *      Author: jdutra
 */

#ifndef FM_SENSORS_H_
#define FM_SENSORS_H_

float voltage_to_pressure(float v);
float voltage_to_current(float v);
float voltage_to_temperature(float v);

typedef enum {
  THERMISTOR_1,
  THERMISTOR_2,
  THERMISTOR_3,
  THERMISTOR_4,
  PRESSURE_1,
  PRESSURE_2,
  POWER,
  RESET_WIFI,
  OTA_UPDATE,
  NO_SENSOR,
}ESensorType_t;

typedef struct {
  float thermistor_1;
  float thermistor_2;
  float thermistor_3;
  float thermistor_4;
  float pressure_1;
  float pressure_2;
  float power;
}sensors_t;

typedef struct {
  ESensorType_t sensor_type;
  float value;
}fm_sensor_message_t;

void fm_sensors_init();

void fm_sensors_get_latest(sensors_t* res);

ESensorType_t fm_sensors_read();

void fm_read_all_sensors(void);

void fm_sensors_close();

fm_sensor_message_t pack_message(ESensorType_t sensor, float vout);

void get_sensor_name_string(ESensorType_t sensor, char * buf);

uint8_t fm_is_mqtt_connected();
#endif /* FM_SENSORS_H_ */
