/***************************************************************************/ /**
 * @file
 * @brief Wi-Fi Provisioning via Access Point Example Application
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include <resources/dashboard2.h>
#include "stdbool.h"
#include "sl_net.h"
#include "app_ap_sel.h"
#include "errno.h"
#include "sl_utility.h"
#include "sl_wifi.h"
#include "sl_net_types.h"
#include "sl_net_wifi_types.h"
#include "sl_si91x_socket_support.h"
#include "sl_si91x_socket_constants.h"
#include "sl_wifi_callback_framework.h"
#include "sl_si91x_socket.h"
#include "sl_net_si91x.h"
#include "login.h"
#include "provisioning.h"
#include "jsmn.h"
#include "sl_http_server.h"
//#include "resources/wifi_provisioning.h"
#include "nvm3_default_config.h"
#include "nvm3_default.h"
#include "nvm3.h"
#include "nvm3_hal_flash.h"
#include "fm_nvm_keys.h"
#include "fm_nvm.h"


/******************************************************
 *                      Macros
 ******************************************************/

// HTTP Status Codes
#define METHOD_BAD_REQUEST           "400 Bad Request"
#define METHOD_NOT_ALLOWED           "405 Not Allowed"
#define METHOD_INTERNAL_SERVER_ERROR "500 Internal Server Error"

// HTTP Chunk Size
#define HTTP_CHUNK_SIZE 512

// Default HTTP Response for Method Not Allowed
#define DEFAULT_HTTP_RESPONSE_METHOD_NOT_ALLOWED                                                                  \
  {                                                                                                               \
    .response_code = SL_HTTP_RESPONSE_METHOD_NOT_ALLOWED, .content_type = SL_HTTP_CONTENT_TYPE_TEXT_HTML,         \
    .headers = NULL, .header_count = 0, .data = (uint8_t *)METHOD_NOT_ALLOWED,                                    \
    .current_data_length = sizeof(METHOD_NOT_ALLOWED) - 1, .expected_data_length = sizeof(METHOD_NOT_ALLOWED) - 1 \
  }

// HTTP Server Port
#define HTTP_SERVER_PORT 80

// Server port number
#define SERVER_PORT 5000

// Buffer size for storing scan results
#define SCAN_RESULT_BUFFER_SIZE (2000)

// Constants for various states and settings
#define ON            "on"
#define OFF           "off"
#define OPEN          "Open"
#define WPA           "WPA"
#define WPA2          "WPA2"
#define WPA3          "WPA3"
#define MIXED_MODE    "Mixed Mode"
#define UNKNOWN       "Unknown"
#define SSID          "ssid"
#define SECURITY_TYPE "security_type"
#define PASSPHRASE    "passphrase"
#define MQTT_SERVER   "server"
#define MQTT_USERNAME "username"
#define MQTT_PASSWORD "password"
#define DEVICE_ID     "A000001"
#define CONFIG_REQUEST_BUFFER_SIZE 512

extern program_data_t pdata;

// Enumeration for states in the application
typedef enum {
  PROVISIONING_INIT_STATE,
  PROVISIONING_STATE,
  CONNECTING_STATE,
  COMPLETED_STATE,
  DISCONNECTING_STATE,
} app_state_t;

/******************************************************
 *               Function Declarations
 ******************************************************/

// Access Point (AP) event handlers
static sl_status_t ap_connected_event_handler(sl_wifi_event_t event,sl_status_t status_code, void *data, uint32_t data_length, void *arg);
static sl_status_t ap_disconnected_event_handler(sl_wifi_event_t event, sl_status_t status_code, void *data, uint32_t data_length, void *arg);


// Security type conversion functions
static sl_wifi_security_t string_to_security_type(const char *security_type);
static char *security_type_to_string(sl_wifi_security_t security_type);

// HTTP server request handlers
static sl_status_t index_request_handler(sl_http_server_t *handle, sl_http_server_request_t *req);
static sl_status_t default_handler(sl_http_server_t *handle, sl_http_server_request_t *req);
static sl_status_t connect_page_request_handler(sl_http_server_t *handle, sl_http_server_request_t *req);
// static sl_status_t connect_data_handler(sl_http_server_t *handle, sl_http_server_request_t *req);
static sl_status_t wifi_scan_request_handler(sl_http_server_t *handle, sl_http_server_request_t *req);
// static sl_status_t mqtt_settings_data_handler(sl_http_server_t *handle, sl_http_server_request_t *req);
static sl_status_t config_handler(sl_http_server_t *handle, sl_http_server_request_t *req);
static bool extract_json_string(const char *json, const char *key, char *out, size_t out_size);

/******************************************************
 *               Variable Definitions
 ******************************************************/
static bool newMQTT_data                                       = false;
static bool scan_complete                                      = false;
static bool disconnect_complete                                = false;
//static uint8_t retry                                           = 0;
static sl_http_server_t server_handle                          = { 0 };
static sl_status_t callback_status                             = SL_STATUS_OK;
static app_state_t app_state                                   = PROVISIONING_INIT_STATE;
static sl_wifi_client_configuration_t provisioned_access_point = { 0 };
static uint8_t connection_status                                = 0U;


static sl_net_wifi_client_profile_t wifi_client_profile_local = {
    .config = {
        .channel.channel = SL_WIFI_AUTO_CHANNEL,
        .channel.band = SL_WIFI_AUTO_BAND,
        .channel.bandwidth = SL_WIFI_AUTO_BANDWIDTH,
        .bssid = {{0}},
        .bss_type = SL_WIFI_BSS_TYPE_INFRASTRUCTURE,
        .client_options = 0,
        .credential_id = SL_NET_DEFAULT_WIFI_CLIENT_CREDENTIAL_ID,
    },
    .ip = {
        .mode = SL_IP_MANAGEMENT_DHCP,
        .type = SL_IPV4,
        .host_name = NULL,
        .ip = {{{0}}},
    }
};

extern sl_wifi_device_configuration_t wifi_mqtt_client_configuration;

static const sl_http_server_handler_t provisioning_server_request_handlers[] = {
  { .uri = "/", .handler = index_request_handler },
  { .uri = "/index.html", .handler = index_request_handler },
  { .uri = "/connect.html", .handler = connect_page_request_handler },
  //{ .uri = "/connect", .handler = connect_data_handler },
  { .uri = "/scan", .handler = wifi_scan_request_handler },
 // { .uri = "/mqtt", .handler = mqtt_settings_data_handler },
  { .uri = "/config", .handler = config_handler }
};

/******************************************************
 *               Function Definitions
 ******************************************************/

uint8_t is_wifi_connected()
{
  if(connection_status == 1){
    return 1U;
  }
  else{
      return 0U;
  }
}

//static sl_status_t join_callback_handler(sl_wifi_event_t event, char *result, uint32_t result_length, void *arg)
//{
//  UNUSED_PARAMETER(result);
//  UNUSED_PARAMETER(arg);
//  printf("in join CB\r\n");
//  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
//    printf("F: Join Event received with %lu bytes payload\n", result_length);
//    return SL_STATUS_FAIL;
//  }
//  return SL_STATUS_OK;
//}

void fm_ap_sel_start(void)
{
  sl_status_t status                    = SL_STATUS_OK;
//  sl_net_wifi_client_profile_t profile  = { 0 };
//  sl_ip_address_t ip_address            = { 0 };
  sl_http_server_config_t server_config = { 0 };

  printf("\r\nWi-Fi Provisioning started\r\n");
  bool finished = false;
  app_state = PROVISIONING_INIT_STATE;
  while (finished == false) {
    switch (app_state) {
      case PROVISIONING_INIT_STATE: {
        // Initialize and start Wi-Fi AP (Access Point) interface
        sl_net_wifi_ap_profile_t ap_profile;

        // Initialize the Wi-Fi AP interface with default configuration
        status = sl_net_init(SL_NET_WIFI_AP_INTERFACE, (const void *)&sl_wifi_default_ap_configuration, NULL, NULL);
        if (status == SL_STATUS_ALREADY_INITIALIZED) {
          printf("Interface already initialised\r\n");
        }
        else if (status != SL_STATUS_OK) {
          printf("Failed to start Wi-Fi AP interface: 0x%lx\r\n", status);
          return;
        }

        // Set callbacks for AP client connection and disconnection events
        sl_wifi_set_callback_v2(SL_WIFI_CLIENT_CONNECTED_EVENTS, ap_connected_event_handler, NULL);
        sl_wifi_set_callback_v2(SL_WIFI_CLIENT_DISCONNECTED_EVENTS, ap_disconnected_event_handler, NULL);
        printf("Wi-Fi AP initialized\r\n");

        // Bring the Wi-Fi AP interface up
        status = sl_net_up(SL_NET_WIFI_AP_INTERFACE, SL_NET_DEFAULT_WIFI_AP_PROFILE_ID);
        if (status != SL_STATUS_OK) {
          printf("Failed to bring Wi-Fi AP interface up: 0x%lx\r\n", status);
          return;
        }
        printf("Wi-Fi AP started\r\n");

        // Configure and start the HTTP server for provisioning
        server_config.port            = HTTP_SERVER_PORT;
        server_config.default_handler = default_handler;
        server_config.handlers_list   = (sl_http_server_handler_t *)provisioning_server_request_handlers;
        server_config.handlers_count  = sizeof(provisioning_server_request_handlers) / sizeof(sl_http_server_handler_t);
        server_config.client_idle_time = 1; // 1 second timeout

        status = sl_http_server_init(&server_handle, &server_config);
        if (status != SL_STATUS_OK) {
          printf("HTTP server init failed:%lx\r\n", status);
          return;
        }

        status = sl_http_server_start(&server_handle);
        if (status != SL_STATUS_OK) {
          printf("Server start fail:%lx\r\n", status);
          return;
        }
        printf("Provisioning HTTP server started\r\n");

        sl_net_get_profile(SL_NET_WIFI_AP_INTERFACE,
                           SL_NET_DEFAULT_WIFI_AP_PROFILE_ID,
                           (sl_net_profile_t *)&ap_profile);
        printf("\r\nConnect to access point \"%s\" from your device\r\n", ap_profile.config.ssid.value);
        printf("Go to http://%u.%u.%u.%u/ on your browser to provisioning of Wi-Fi\r\n",
               ap_profile.ip.ip.v4.ip_address.bytes[0],
               ap_profile.ip.ip.v4.ip_address.bytes[1],
               ap_profile.ip.ip.v4.ip_address.bytes[2],
               ap_profile.ip.ip.v4.ip_address.bytes[3]);
        app_state = PROVISIONING_STATE;
        break;
      }
      case CONNECTING_STATE: {
        // Transition from AP provisioning to client connection
        // Stop and deinitialize the HTTP server
        status = sl_http_server_stop(&server_handle);
        if (status != SL_STATUS_OK) {
          printf("Server stop fail:%lx\r\n", status);
          return;
        }
        status = sl_http_server_deinit(&server_handle);
        if (status != SL_STATUS_OK) {
          printf("Server deinit fail:%lx\r\n", status);
          return;
        }
        printf("HTTP Server deinitialized\r\n");

        status = sl_net_deinit(SL_NET_WIFI_AP_INTERFACE);
        if (status != SL_STATUS_OK) {
          printf("Ap deinit : 0x%lx\r\n", status);
          return;
        }
        printf("Wi-Fi AP deinitialized\r\n");

        // Initialize the Wi-Fi client interface
        // status = sl_net_init(SL_NET_WIFI_CLIENT_INTERFACE, &wifi_mqtt_client_configuration, NULL, NULL);
        // if (status != SL_STATUS_OK) {
        //   printf("Failed to start Wi-Fi client interface: 0x%lx\r\n", status);
        //   return;
        // }
        // printf("Wi-Fi client interface initialized\r\n");

        sl_wifi_credential_t cred  = { 0 };
        sl_wifi_credential_id_t id = 2; //SL_NET_DEFAULT_WIFI_CLIENT_CREDENTIAL_ID;
        cred.type                  = SL_WIFI_PSK_CREDENTIAL;
        memcpy(cred.psk.value, pdata.wifi_client_credential, strlen((char *)pdata.wifi_client_credential));

        status =
          sl_net_set_credential(id, SL_NET_WIFI_PSK, pdata.wifi_client_credential, strlen((char *)pdata.wifi_client_credential));

        memset(&provisioned_access_point, 0, sizeof(provisioned_access_point));
        provisioned_access_point.ssid.length = strlen((char *)pdata.wifi_client_profile_ssid);
        memcpy(provisioned_access_point.ssid.value, pdata.wifi_client_profile_ssid, provisioned_access_point.ssid.length);
        provisioned_access_point.security      = string_to_security_type(pdata.wifi_client_security_type);
        provisioned_access_point.encryption    = SL_WIFI_CCMP_ENCRYPTION;
        provisioned_access_point.credential_id = id;
        printf("Security Type:%s\n",security_type_to_string(provisioned_access_point.security));

        //  Keeping the station ipv4 record in profile_id_0
        memcpy(&wifi_client_profile_local.config, &provisioned_access_point, sizeof(provisioned_access_point));
        memcpy(&pdata.profile, &wifi_client_profile_local, sizeof(wifi_client_profile_local));

        fm_write_data(&pdata);

        app_state = COMPLETED_STATE;
        break;
      }
      case DISCONNECTING_STATE: {
        printf("Wi-Fi Provisioning demo is successfully completed\r\n");
        disconnect_complete = true;
        break;
      }
      case PROVISIONING_STATE:
        // Delay for 1 second
        osDelay(500);
        if(newMQTT_data)
          {
            printf("saving mqtt settings to nvm...\n");
            fm_write_data(&pdata);
            newMQTT_data = false;
          }
        break;
      case COMPLETED_STATE: {
        finished = true;
        break;
      }
    }
    if (disconnect_complete) {
      break;
    }
    osDelay(100);
  }
}

static sl_status_t ap_connected_event_handler(sl_wifi_event_t event,
                                              sl_status_t status_code,
                                              void *data,
                                              uint32_t data_length,
                                              void *arg)
{
  UNUSED_PARAMETER(data_length);
  UNUSED_PARAMETER(arg);

  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
    return status_code;
  }
  
  printf("Remote Client connected: ");
  print_mac_address((sl_mac_address_t *)data);
  printf("\r\n");
  return SL_STATUS_OK;
}

static sl_status_t ap_disconnected_event_handler(sl_wifi_event_t event,
                                                 sl_status_t status_code,
                                                 void *data,
                                                 uint32_t data_length,
                                                 void *arg)
{
  UNUSED_PARAMETER(data_length);
  UNUSED_PARAMETER(arg);

  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
    return status_code;
  }

  printf("Remote Client disconnected: ");
  print_mac_address((sl_mac_address_t *)data);
  printf("\r\n");

  return SL_STATUS_OK;
}

static sl_status_t index_request_handler(sl_http_server_t *handle, sl_http_server_request_t *req)
{
  sl_http_server_response_t http_response = DEFAULT_HTTP_RESPONSE_METHOD_NOT_ALLOWED;

  printf("Got request %s with data length : %lu\r\n", req->uri.path, req->request_data_length);

  // Handle GET requests for the index page
  if (req->type == SL_HTTP_REQUEST_GET) {

    http_response.response_code        = SL_HTTP_RESPONSE_OK;
    http_response.content_type         = SL_HTTP_CONTENT_TYPE_TEXT_HTML;
    uint32_t data_length               = wifi_provisioning_html_len;
    uint32_t tx_length                 = (data_length > HTTP_CHUNK_SIZE ? HTTP_CHUNK_SIZE : data_length);
    http_response.data                 = (uint8_t *)wifi_provisioning_html;
    http_response.current_data_length  = tx_length;
    http_response.expected_data_length = data_length;

    // Send the first chunk
    sl_http_server_send_response(handle, &http_response);

    // Update the remaining data length
    data_length -= tx_length;

    while (data_length > 0) {
      // Calculate the next chunk size (handle case where the remaining data is less than 1024 bytes)
      tx_length = (data_length > HTTP_CHUNK_SIZE ? HTTP_CHUNK_SIZE : data_length);

      // Update the data pointer after sending the current chunk
      http_response.data += http_response.current_data_length;

      // Send the current chunk
      sl_http_server_write_data(handle, http_response.data, tx_length);

      // Update the remaining data length
      data_length -= tx_length;

      // Update the current chunk length for next iteration
      http_response.current_data_length = tx_length;
    }
    // Send the last chunk if there is any remaining data
    if (data_length > 0) {
      http_response.data += tx_length;
      http_response.current_data_length = data_length;
      sl_http_server_write_data(handle, http_response.data, data_length);
    }
  } else {
    // Send the HTTP response
    sl_http_server_send_response(handle, &http_response);
  }
  return SL_STATUS_OK;
}

static sl_status_t connect_page_request_handler(sl_http_server_t *handle, sl_http_server_request_t *req)
{
  sl_http_server_response_t http_response = DEFAULT_HTTP_RESPONSE_METHOD_NOT_ALLOWED;

  printf("Got request %s with data length : %lu\r\n", req->uri.path, req->request_data_length);

  // Handle GET requests for the connect page
  if (req->type == SL_HTTP_REQUEST_GET) {
    http_response.response_code        = SL_HTTP_RESPONSE_OK;
    http_response.content_type         = SL_HTTP_CONTENT_TYPE_TEXT_HTML;
    http_response.data                 = (uint8_t *)login_content;
    http_response.current_data_length  = strlen((const char *)login_content);
    http_response.expected_data_length = http_response.current_data_length;
  }
  // Send the HTTP response
  sl_http_server_send_response(handle, &http_response);
  return SL_STATUS_OK;
}


static sl_status_t wlan_app_scan_callback_handler(sl_wifi_event_t event,
                                                  sl_wifi_scan_result_t *scan_result,
                                                  uint32_t result_length,
                                                  void *arg)
{
  char *scan_result_buffer = (char *)arg;
  uint8_t *bssid           = NULL;
  UNUSED_PARAMETER(arg);
  UNUSED_PARAMETER(result_length);

  // Check if the scan event indicates failure
  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
    callback_status = *(sl_status_t *)scan_result;
    return SL_STATUS_FAIL;
  }

  if (scan_result->scan_count) {
    uint32_t buffer_length = SCAN_RESULT_BUFFER_SIZE - 1;
    int32_t index =
      snprintf(scan_result_buffer, buffer_length, "{\"count\": \"%lu\", \"scan_results\": [", scan_result->scan_count);
    scan_result_buffer += index;
    buffer_length -= index;

    // Iterate through scan results and format them as JSON
    for (uint32_t a = 0; a < scan_result->scan_count; a++) {
      bssid = (uint8_t *)&scan_result->scan_info[a].bssid;
      index = snprintf(scan_result_buffer,
                       buffer_length,
                       "{\"ssid\": \"%s\", \"security_type\": \"%s\", \"network_type\": \"%u\",",
                       scan_result->scan_info[a].ssid,
                       security_type_to_string(scan_result->scan_info[a].security_mode),
                       scan_result->scan_info[a].network_type);
      scan_result_buffer += index;
      buffer_length -= index;
      index = snprintf(scan_result_buffer,
                       buffer_length,
                       " \"bssid\": \"%02x:%02x:%02x:%02x:%02x:%02x\", \"channel\": \"%u\", \"rssi\": \"-%u\"}",
                       bssid[0],
                       bssid[1],
                       bssid[2],
                       bssid[3],
                       bssid[4],
                       bssid[5],
                       scan_result->scan_info[a].rf_channel,
                       scan_result->scan_info[a].rssi_val);
      scan_result_buffer += index;
      buffer_length -= index;
      if (a < scan_result->scan_count - 1) {
        index = snprintf(scan_result_buffer, buffer_length, ",");
        scan_result_buffer += index;
        buffer_length -= index;
      }
    }
    index = snprintf(scan_result_buffer, buffer_length, "]}");
    scan_result_buffer += index;
    buffer_length -= index;
  }

  scan_complete = true; // Indicate that the scan is complete
  return SL_STATUS_OK;
}

static sl_status_t wifi_scan_request_handler(sl_http_server_t *handle, sl_http_server_request_t *req)
{
  sl_http_server_response_t http_response = DEFAULT_HTTP_RESPONSE_METHOD_NOT_ALLOWED;

  // Handle GET requests for Wi-Fi scan results
  if (req->type == SL_HTTP_REQUEST_GET) {
    sl_status_t status;
    char *scan_result_buffer                             = (char *)malloc(SCAN_RESULT_BUFFER_SIZE);
    sl_wifi_scan_configuration_t wifi_scan_configuration = default_wifi_scan_configuration;

    printf("Got request %s with data length : %lu\r\n", req->uri.path, req->request_data_length);
    memset(scan_result_buffer, 0, SCAN_RESULT_BUFFER_SIZE);

    printf("WLAN scan started \r\n");
    scan_complete = false;
    sl_wifi_set_scan_callback(wlan_app_scan_callback_handler, (void *)scan_result_buffer);
    status = sl_wifi_start_scan(SL_WIFI_AP_INTERFACE, NULL, &wifi_scan_configuration);
    if (SL_STATUS_IN_PROGRESS == status) {
      printf("Scanning...\r\n");
      const uint32_t start = osKernelGetTickCount();

      // Wait for scan completion or timeout
      while (!scan_complete && (osKernelGetTickCount() - start) <= 10000) {
        osThreadYield();
      }
      status = scan_complete ? callback_status : SL_STATUS_TIMEOUT;
    }
    if (status != RSI_SUCCESS) {
      printf("WLAN Scan failed %lx. Please make sure the latest connectivity firmware is used.\r\n", status);
      http_response.response_code        = SL_HTTP_RESPONSE_INTERNAL_SERVER_ERROR;
      http_response.data                 = (uint8_t *)METHOD_INTERNAL_SERVER_ERROR;
      http_response.current_data_length  = sizeof(METHOD_INTERNAL_SERVER_ERROR) - 1;
      http_response.expected_data_length = http_response.current_data_length;

      sl_http_server_send_response(handle, &http_response);
    } else {
      printf("Scan done state \r\n");
      uint32_t data_length               = strlen((const char *)scan_result_buffer);
      uint32_t tx_length                 = (data_length > HTTP_CHUNK_SIZE ? HTTP_CHUNK_SIZE : data_length);
      http_response.response_code        = SL_HTTP_RESPONSE_OK;
      http_response.content_type         = SL_HTTP_CONTENT_TYPE_APPLICATION_JSON;
      http_response.data                 = (uint8_t *)scan_result_buffer;
      http_response.current_data_length  = tx_length;
      http_response.expected_data_length = data_length;

      sl_http_server_send_response(handle, &http_response); // Send response
      data_length -= tx_length;
      while (data_length > 0) {
        tx_length = (data_length > HTTP_CHUNK_SIZE ? HTTP_CHUNK_SIZE : data_length);

        // Update the data pointer after sending the current chunk
        http_response.data += http_response.current_data_length;

        // Send the current chunk
        sl_http_server_write_data(handle, http_response.data, tx_length);

        // Update the remaining data length
        data_length -= tx_length;

        // Update the current chunk length for next iteration
        http_response.current_data_length = tx_length;
      }
      // Send the last chunk if there is any remaining data
      if (data_length > 0) {
        http_response.data += tx_length;
        http_response.current_data_length = data_length;
        sl_http_server_write_data(handle, http_response.data, data_length);
      }
    }

    free(scan_result_buffer); // Free allocated buffer
  } else {
    sl_http_server_send_response(handle, &http_response);
  }
  return SL_STATUS_OK;
}

static sl_status_t config_handler(sl_http_server_t *handle, sl_http_server_request_t *req)
{
  sl_http_server_response_t http_response = DEFAULT_HTTP_RESPONSE_METHOD_NOT_ALLOWED;
  if (req->type == SL_HTTP_REQUEST_GET)
  {
    printf("Got HTTP request for configuration data\r\n");

    sl_http_header_t header = {.key = "Server", .value = "SI917-HTTPServer"};

    http_response.response_code = SL_HTTP_RESPONSE_OK;
    http_response.content_type = SL_HTTP_CONTENT_TYPE_APPLICATION_JSON;
    http_response.headers = &header;
    http_response.header_count = 1;

    char json[512];
    int written = snprintf(json, sizeof(json),
                           "{\"ssid\":\"%s\",\"security_type\":\"%s\","
                           "\"mqtt_server\":\"%s\",\"mqtt_username\":\"%s\"}",
                           pdata.wifi_client_profile_ssid, security_type_to_string(pdata.profile.config.security),
                           pdata.mqtt_server_address, pdata.mqtt_client_username);

    http_response.data = (uint8_t *)json;
    http_response.current_data_length = (uint32_t)written;
    http_response.expected_data_length = (uint32_t)written;

    sl_http_server_write_data(handle, http_response.data, sizeof(http_response.data));
    sl_http_server_send_response(handle, &http_response);
  }
  else if (req->type == SL_HTTP_REQUEST_POST)
  {
    printf("Received configuration data from user\r\n");
    if (req->request_data_length == 0 || req->request_data_length >= CONFIG_REQUEST_BUFFER_SIZE)
    {
      http_response.response_code = SL_HTTP_RESPONSE_BAD_REQUEST;
      sl_http_server_send_response(handle, &http_response);
      return SL_STATUS_OK;
    }

    char *body_buffer = (char *)malloc(CONFIG_REQUEST_BUFFER_SIZE);
    if (body_buffer == NULL)
    {
      http_response.response_code = SL_HTTP_RESPONSE_INTERNAL_SERVER_ERROR;
      sl_http_server_send_response(handle, &http_response);
      return SL_STATUS_OK;
    }
    memset(body_buffer, 0, CONFIG_REQUEST_BUFFER_SIZE);

    sl_http_recv_req_data_t recv_data = {
        .request = req,
        .buffer = (uint8_t *)body_buffer,
        .buffer_length = CONFIG_REQUEST_BUFFER_SIZE - 1,
        .received_data_length = 0,
    };

    sl_status_t status = sl_http_server_read_request_data(handle, &recv_data);
    if (status != SL_STATUS_OK)
    {
      free(body_buffer);
      http_response.response_code = SL_HTTP_RESPONSE_INTERNAL_SERVER_ERROR;
      sl_http_server_send_response(handle, &http_response);
      return SL_STATUS_OK;
    }

    char temp[64];

    if (extract_json_string(body_buffer, "ssid", temp, sizeof(temp)))
    {
      strncpy(pdata.wifi_client_profile_ssid, temp, sizeof(pdata.wifi_client_profile_ssid) - 1);
    }
    if (extract_json_string(body_buffer, "security_type", temp, sizeof(temp)))
    {
      strncpy(pdata.wifi_client_security_type, temp, sizeof(pdata.wifi_client_security_type));
    }

    if (extract_json_string(body_buffer, "passphrase", temp, sizeof(temp)))
    {
      strncpy(pdata.wifi_client_credential, temp, sizeof(pdata.wifi_client_credential));
    }

    if (extract_json_string(body_buffer, "server", temp, sizeof(temp)))
    {
      strncpy(pdata.mqtt_server_address, temp, sizeof(pdata.mqtt_server_address) - 1);
    }
    if (extract_json_string(body_buffer, "username", temp, sizeof(temp)))
    {
      strncpy(pdata.mqtt_client_username, temp, sizeof(pdata.mqtt_client_username) - 1);
    }

    // Only overwrite the stored MQTT password if the client actually sent a non-empty one
    // — an empty field from the browser means "keep the current password".
    char mqtt_password[64] = {0};
    if (extract_json_string(body_buffer, "password", mqtt_password, sizeof(mqtt_password)) && strlen(mqtt_password) > 0)
    {
      strncpy(pdata.mqtt_client_password, mqtt_password, sizeof(pdata.mqtt_client_password) - 1);
    }

    free(body_buffer);

    // TODO: kick off sl_wifi_connect / MQTT reconnect using g_device_config + passphrase here
    // TODO: validate data. maybe in the fm_write_data function?
    // fm_write_data(&pdata);
    osDelay(500);
    app_state = CONNECTING_STATE; // we're not actually connected here, just tell the state manager that we to force an exit

    http_response.response_code = SL_HTTP_RESPONSE_OK;
    http_response.content_type = SL_HTTP_CONTENT_TYPE_APPLICATION_JSON;
    http_response.data = (uint8_t *)"{\"status\":\"ok\"}";
    http_response.current_data_length = strlen("{\"status\":\"ok\"}");
    http_response.expected_data_length = http_response.current_data_length;
    sl_http_server_send_response(handle, &http_response);

    return SL_STATUS_OK;
  }
  else
  {
    sl_http_server_send_response(handle, &http_response);
    return SL_STATUS_OK;
  }
  return SL_STATUS_OK;
}

sl_status_t default_handler(sl_http_server_t *handle, sl_http_server_request_t *req)
{
  sl_http_server_response_t http_response = DEFAULT_HTTP_RESPONSE_METHOD_NOT_ALLOWED;
  sl_http_header_t header                 = { .key = "Server", .value = "SI917-HTTPServer" };

  UNUSED_PARAMETER(req);

  // Handle requests for unknown or unsupported URIs
  http_response.response_code = SL_HTTP_RESPONSE_NOT_FOUND;
  http_response.content_type  = SL_HTTP_CONTENT_TYPE_TEXT_PLAIN;
  http_response.headers       = &header;
  http_response.header_count  = 1;

  char *response_data                = "404 Not Found";
  http_response.data                 = (uint8_t *)response_data;
  http_response.current_data_length  = strlen(response_data);
  http_response.expected_data_length = http_response.current_data_length;
  sl_http_server_send_response(handle, &http_response);

  return SL_STATUS_OK;
}

static bool extract_json_string(const char *json, const char *key, char *out, size_t out_size)
{
  char search_key[48];
  snprintf(search_key, sizeof(search_key), "\"%s\"", key);

  const char *key_pos = strstr(json, search_key);
  if (key_pos == NULL) {
    return false;
  }

  const char *colon = strchr(key_pos, ':');
  if (colon == NULL) {
    return false;
  }

  const char *value_start = strchr(colon, '\"');
  if (value_start == NULL) {
    return false;
  }
  value_start++; // skip opening quote

  const char *value_end = strchr(value_start, '\"');
  if (value_end == NULL) {
    return false;
  }

  size_t value_len = (size_t)(value_end - value_start);
  if (value_len >= out_size) {
    value_len = out_size - 1;
  }

  memcpy(out, value_start, value_len);
  out[value_len] = '\0';
  return true;
}

static sl_wifi_security_t string_to_security_type(const char *security_type)
{
  // Compare the input string to known security type strings
  if (strcmp(security_type, OPEN) == 0) {
    return SL_WIFI_OPEN;
  } else if (strcmp(security_type, WPA) == 0) {
    return SL_WIFI_WPA;
  } else if (strcmp(security_type, WPA2) == 0) {
    return SL_WIFI_WPA2;
  } else if (strcmp(security_type, WPA3) == 0) {
    return SL_WIFI_WPA3;
  } else if (strcmp(security_type, MIXED_MODE) == 0) {
    return SL_WIFI_WPA_WPA2_MIXED;
  } else {
    return SL_WIFI_SECURITY_UNKNOWN;
  }
}

static char *security_type_to_string(sl_wifi_security_t security_type)
{
  // Return the string representation for the given enum value
  switch (security_type) {
    case SL_WIFI_OPEN:
      return OPEN;
    case SL_WIFI_WPA:
      return WPA;
    case SL_WIFI_WPA2:
      return WPA2;
    case SL_WIFI_WPA3:
      return WPA3;
    case SL_WIFI_WPA_WPA2_MIXED:
      return MIXED_MODE;
    default:
      return UNKNOWN;
  }
}
