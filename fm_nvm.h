#ifndef FM_NVM_H
#define FM_NVH_H

#include "fm_error_codes.h"
#include "sl_net_wifi_types.h"

/**
 * ------------- TYPES -------------
 */
typedef struct {
    sl_net_wifi_client_profile_t profile;
    char wifi_client_profile_ssid[32]; // Assuming SSID can be up to 32 characters long
    char wifi_client_credential[64];   // Assuming Password can be up to 64 characters long
    char wifi_client_security_type[32];
    char mqtt_client_username[32];
    char mqtt_client_password[32];
    char mqtt_server_address[64];
    char device_id[32];
}program_data_t;

/**
 * ----------- FUNCTIONS -------------------
 */

 fm_error_t fm_nvm_init(void);
 fm_error_t fm_read_data(program_data_t *data);
 fm_error_t fm_write_data(program_data_t * const data);

#endif