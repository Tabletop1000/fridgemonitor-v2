/*
 * fm_sensor_types.h
 *
 *  Created on: 19 Feb 2025
 *      Author: jdutra
 */

#ifndef FM_SENSOR_TYPES_H_
#define FM_SENSOR_TYPES_H_

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
