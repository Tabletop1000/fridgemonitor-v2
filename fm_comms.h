/*
 * fm_comms.h
 *
 *  Created on: 16 Feb 2025
 *      Author: jdutra
 */

#ifndef FM_COMMS_H_
#define FM_COMMS_H_
#include <fm_error_codes.h>

fm_error_t fm_comms_init();
fm_error_t fm_comms_connect();
fm_error_t fm_comms_mqtt_start();
fm_error_t fm_comms_mqtt_stop();
void fm_comms_deint();
fm_error_t fm_comms_publish_data(const char * data, size_t len, const char * topic, size_t topic_len, const char * device_id, size_t device_id_len);
uint8_t fm_is_mqtt_started();
uint8_t fm_is_wifi_connected();

#endif /* FM_COMMS_H_ */
