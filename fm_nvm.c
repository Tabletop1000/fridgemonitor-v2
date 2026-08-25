#include "nvm3_default_config.h"
#include "nvm3_default.h"
#include "nvm3.h"
#include "nvm3_hal_flash.h"
#include "fm_nvm_keys.h"
#include "sl_net_wifi_types.h"
#include "fm_error_codes.h"
#include "fm_nvm.h"

fm_error_t fm_nvm_init(void)
{
    sl_status_t status;

    status = nvm3_initDefault();
    if (status != SL_STATUS_OK)
    {
        printf("Failed to initialise NVM: 0x%lx\r\n", status);
        return FM_NVM_ERROR;
    }
    return FM_SUCCESS;
}

fm_error_t fm_read_data(program_data_t *data)
{
    Ecode_t nvm_status;

    size_t num_objects = 0;

    num_objects = nvm3_countObjects(nvm3_defaultHandle);

    if (num_objects < 5)
    {
        printf("No objects in non-volatile memory\r\n");
        return FM_NVM_EMPTY;
    }
    printf("Found %d object(s) in non-volatile memory\r\n", num_objects);
    nvm_status = nvm3_readData(nvm3_defaultHandle,
                               NVM3_KEY_WIFI_PROFILE,
                               &data->profile,
                               sizeof(data->profile));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to read profile from nvm: 0x%lx\r\n", nvm_status);
        return FM_NVM_ERROR;
    }
    nvm_status = nvm3_readData(nvm3_defaultHandle,
                               NVM3_KEY_WIFI_CREDENTIAL,
                               &data->wifi_client_credential,
                               sizeof(data->wifi_client_credential));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to read credentials from nvm: 0x%lx\r\n", nvm_status);
        return FM_NVM_ERROR;
    }
    // Fetch the MQTT server address
    nvm_status = nvm3_readData(nvm3_defaultHandle,
                               NVM3_KEY_MQTT_ADDRESS,
                               &data->mqtt_server_address,
                               sizeof(data->mqtt_server_address));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to mqtt broker IP from nvm: 0x%lx\r\n", nvm_status);
        return FM_NVM_ERROR;
    }
    // Fetch the MQTT client username
    nvm_status = nvm3_readData(nvm3_defaultHandle,
                               NVM3_KEY_MQTT_USERNAME,
                               &data->mqtt_client_username,
                               sizeof(data->mqtt_client_username));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to mqtt username from nvm: 0x%lx\r\n", nvm_status);
        return FM_NVM_ERROR;
    }
    // Fetch the MQTT client password
    nvm_status = nvm3_readData(nvm3_defaultHandle,
                               NVM3_KEY_MQTT_PASSWORD,
                               data->mqtt_client_password,
                               sizeof(data->mqtt_client_password));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to mqtt password from nvm: 0x%lx\r\n", nvm_status);
        return FM_NVM_ERROR;
    }
    // Fetch the device ID
    nvm_status = nvm3_readData(nvm3_defaultHandle,
                               NVM3_KEY_MQTT_PASSWORD,
                               data->mqtt_client_password,
                               sizeof(data->mqtt_client_password));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to mqtt password from nvm: 0x%lx\r\n", nvm_status);
        return FM_NVM_ERROR;
    }
    return FM_SUCCESS;
}

fm_error_t fm_write_data(program_data_t * const data)
{
    printf("Writing wifi data to nvm...\n");
    Ecode_t nvm_status;
    nvm_status = nvm3_writeData(nvm3_defaultHandle, NVM3_KEY_WIFI_PROFILE, &data->profile, sizeof(data->profile));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to write profile from nvm: 0x%lx\r\n", nvm_status);
    }
    nvm_status = nvm3_writeData(nvm3_defaultHandle, NVM3_KEY_WIFI_CREDENTIAL, &data->wifi_client_credential, sizeof(data->wifi_client_credential));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to write profile from nvm: 0x%lx\r\n", nvm_status);
    }
    nvm_status = nvm3_writeData(nvm3_defaultHandle, NVM3_KEY_MQTT_ADDRESS, data->mqtt_server_address, sizeof(data->mqtt_server_address));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to write mqtt broker IP nvm: 0x%lx\r\n", nvm_status);
    }
    nvm_status = nvm3_writeData(nvm3_defaultHandle, NVM3_KEY_MQTT_USERNAME, data->mqtt_client_username, sizeof(data->mqtt_client_username));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to write mqtt username nvm: 0x%lx\r\n", nvm_status);
    }
    nvm_status = nvm3_writeData(nvm3_defaultHandle, NVM3_KEY_MQTT_PASSWORD, data->mqtt_client_password, sizeof(data->mqtt_client_password));
    if (nvm_status != ECODE_NVM3_OK)
    {
        printf("failed to write mqtt password nvm: 0x%lx\r\n", nvm_status);
    }
    return FM_SUCCESS;
}