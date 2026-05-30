/*
 * fm_comms.h
 *
 *  Created on: 16 Feb 2025
 *      Author: jdutra
 */

#ifndef FM_COMMS_H_
#define FM_COMMS_H_

typedef enum {
  FMCOMMS_SUCCESS,
  FMCOMMS_FAILED,
  FMCOMMS_NVM_ERROR,
  FMCOMMS_NVM_EMPTPY,
  FMCOMMS_WIFI_NOT_FOUND,
  FMCOMMS_NOT_INITIALISED,
  FMCOMMS_OTA_UPDATE,
  FMCOMMS_MQTT_ERROR,
}fm_comms_status;

fm_comms_status fm_comms_init();
fm_comms_status fm_comms_connect();
fm_comms_status fm_comms_mqtt_start();
fm_comms_status fm_comms_mqtt_stop();
void fm_comms_deint();
fm_comms_status fm_comms_publish_data(const char * data, size_t len, const char * topic, size_t topic_len);
uint8_t fm_is_mqtt_started();
uint8_t fm_is_wifi_connected();

#endif /* FM_COMMS_H_ */
