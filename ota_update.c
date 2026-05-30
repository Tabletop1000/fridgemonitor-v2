/*
 * ota_update.c
 *
 *  Created on: 15 Feb 2026
 *      Author: jdutra
 */

#include "cmsis_os2.h"
#include "sl_board_configuration.h"
#include "sl_net.h"
#include "sl_wifi_types.h"
#include <string.h>
#include "sl_wifi.h"
#include "sl_wifi_callback_framework.h"
#include "firmware_upgradation.h"
#include "sl_net_dns.h"
#include "sl_utility.h"
#include "sl_net_si91x.h"
#include "sl_net_wifi_types.h"

// include certificates

#if LOAD_CERTIFICATE
#include "aws_starfield_ca.pem.h"
#include "azure_baltimore_ca.pem.h"
#include "cacert.pem.h"
#endif

#define M4_FW_UPDATE       0
#define TA_FW_UPDATE       1
#define COMBINED_FW_UPDATE 2
// Choose between M4_FW_UPDATE, TA_FW_UPDATE and COMBINED_FW_UPDATE
#define FW_UPDATE_TYPE M4_FW_UPDATE

#ifdef SLI_SI91X_MCU_INTERFACE
 #include "sl_si91x_hal_soc_soft_reset.h"
#endif

//! Set HTTP_V_1_1 to use HTTP version 1.1
#define HTTP_V_1_1 BIT(6)

//! Enable user defined http content type in FLAGS
// #define HTTP_USER_DEFINED_CONTENT_TYPE BIT(7)

// HTTP OTAF
#define HTTP_OTAF 2

//! set 1 for selecting SL_SI91X_HTTPS_CERTIFICATE_INDEX_1, set 2 for selecting SL_SI91X_HTTPS_CERTIFICATE_INDEX_2
#define CERTIFICATE_INDEX 0

#define DNS_TIMEOUT         20000
#define MAX_DNS_RETRY_COUNT 5
#define OTAF_TIMEOUT        600000

#define FLAGS                  0
//! Server port number
#define HTTP_PORT              80
//! HTTP Server IP address.
#define HTTP_SERVER_IP_ADDRESS "192.168.0.49"
//! HTTP resource name
#if (FW_UPDATE_TYPE == TA_FW_UPDATE)
#define HTTP_URL "rps/firmware.rps"
#else
#define HTTP_URL "firmware/fm_firmware.rps"
#endif
//! set HTTP hostname
#define HTTP_HOSTNAME        "192.168.0.49"
char *hostname = HTTP_HOSTNAME;
//! set HTTP extended header
//! if NULL , driver fills default extended header
#define HTTP_EXTENDED_HEADER NULL
//! set HTTP hostname
#define USERNAME             "sensor"
//! set HTTP hostname
#define PASSWORD             "staycool"
#define SERVER_NAME          "Local Apache Server"

#if (FW_UPDATE_TYPE == TA_FW_UPDATE)
  sl_wifi_firmware_version_t version = { 0 };
#endif

uint8_t connection_status = 0U;

// -- FUNCTION DEFFINITIONS --
static sl_status_t http_fw_update_response_handler(sl_wifi_event_t event,
                                                   uint16_t *data,
                                                   uint32_t data_length,
                                                   void *arg);

// -----------------------------

volatile bool response               = false;
volatile sl_status_t callback_status = SL_STATUS_OK;

uint8_t
ota_update() {
  sl_status_t status;
   uint16_t flags     = FLAGS;
   char server_ip[16];

   if (CERTIFICATE_INDEX == 1) {
     flags |= SL_SI91X_HTTPS_CERTIFICATE_INDEX_1;
   } else if (CERTIFICATE_INDEX == 2) {
     flags |= SL_SI91X_HTTPS_CERTIFICATE_INDEX_2;
   }

#if (FW_UPDATE_TYPE == TA_FW_UPDATE)
        status = sl_wifi_get_firmware_version(&version);
        print_firmware_version(&version);
#endif
        //sl_wifi_set_join_callback(join_callback_handler,NULL);
        sl_wifi_set_callback(SL_WIFI_HTTP_OTA_FW_UPDATE_EVENTS,
                             (sl_wifi_callback_function_t)&http_fw_update_response_handler,
                             NULL);


        strcpy(server_ip, HTTP_SERVER_IP_ADDRESS);
        printf("\r\n%s IP Address : %s\r\n", SERVER_NAME, HTTP_HOSTNAME);
        printf("\r\nFirmware download from %s is in progress...\r\n", SERVER_NAME);

        sl_si91x_http_otaf_params_t http_params = { 0 };

        http_params.flags           = (uint16_t)flags;
        http_params.ip_address      = (uint8_t *)server_ip;
        http_params.port            = (uint16_t)HTTP_PORT;
        http_params.resource        = (uint8_t *)HTTP_URL;
        http_params.host_name       = (uint8_t *)hostname;
        http_params.extended_header = (uint8_t *)HTTP_EXTENDED_HEADER;
        http_params.user_name       = (uint8_t *)USERNAME;
        http_params.password        = (uint8_t *)PASSWORD;

        //! OTAF firmware upgrade
        status = sl_si91x_http_otaf_v2(&http_params);
        uint8_t ret = 1U;

        if (status != SL_STATUS_OK) {
          printf("\r\n Firmware update status = 0x%lX\r\n", status);
          ret = 1U;
        } else {
          printf("\r\nCompleted firmware download using %s\r\n", SERVER_NAME);
          printf("\r\nUpdating the firmware...\r\n");
          ret = 0U;
        }
        return ret;
}

/******************************************************
 *               Function Declarations
 ******************************************************/
#if LOAD_CERTIFICATE
static sl_status_t clear_and_load_certificates_in_flash(void);
#endif

/******************************************************
 *               Function Definitions
 ******************************************************/

#if LOAD_CERTIFICATE
sl_status_t clear_and_load_certificates_in_flash(void)
{
  sl_status_t status;
  void *cert           = NULL;
  uint32_t cert_length = 0;

  cert        = (uint8_t *)cacert;
  cert_length = (sizeof(cacert) - 1);

  //! Load SSL CA certificate
  status = sl_net_set_credential(SL_NET_TLS_SERVER_CREDENTIAL_ID(CERTIFICATE_INDEX),
                                 SL_NET_SIGNING_CERTIFICATE,
                                 cert,
                                 cert_length);
  if (status != SL_STATUS_OK) {
    printf("\r\nLoading TLS CA certificate in to FLASH Failed, Error Code : 0x%lX\r\n", status);
  } else {
    printf("\r\nLoad TLS CA certificate at index %d Success\r\n", CERTIFICATE_INDEX);
  }

  return status;
}
#endif


static sl_status_t http_fw_update_response_handler(sl_wifi_event_t event,
                                                   uint16_t *data,
                                                   uint32_t data_length,
                                                   void *arg)
{
  UNUSED_PARAMETER(data);
  UNUSED_PARAMETER(data_length);
  UNUSED_PARAMETER(arg);
  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
    response = false;
    return SL_STATUS_FAIL;
  }
  response = true;
  return SL_STATUS_OK;
}