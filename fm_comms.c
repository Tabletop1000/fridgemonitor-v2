/*
 * fm_comms.c
 *
 *  Created on: 16 Feb 2025
 *      Author: jdutra
 */

#include "rsi_debug.h"
#include <stdio.h>
//FIXME remove un-needed includes
#include "sl_net.h"
#include "sl_net_dns.h"
#include "sl_si91x_types.h"
#include "sl_utility.h"
#include "sl_net_wifi_types.h"
#include "sl_net_default_values.h"
#include "sl_wifi.h"
#include "sl_net_types.h"
#include "sl_net_wifi_types.h"
#include "sl_net_si91x.h"
#include "sl_si91x_socket_support.h"
#include "sl_si91x_socket_constants.h"
#include "sl_wifi_callback_framework.h"
#include "sl_utility.h"
#include "cmsis_os2.h"
#include "sl_constants.h"
#include "sl_mqtt_client.h"
#include "cacert.pem.h"
#include "sl_wifi.h"
#include "string.h"
#include "fm_comms.h"
#include "app_ap_sel.h"
#include "nvm3_default_config.h"
#include "nvm3_default.h"
#include "nvm3.h"
#include "nvm3_hal_flash.h"
#include "fm_nvm_keys.h"


/******************************************************
 *                    Constants
 ******************************************************/

#ifdef SLI_SI91X_ENABLE_IPV6
#define MQTT_BROKER_IP "2401:4901:1290:10de::1000"
#endif

#define MQTT_BROKER_PORT 1884

#define CLIENT_PORT 2

#define CLIENT_ID "FRIDGEMONITOR-MQTT-CLIENT-ID"

#define LAST_WILL_TOPIC       "FRIDGEMONITOR-LAST-WILL"
#define LAST_WILL_MESSAGE     "FRIDGEMONITOR-MQTT-CLIENT has been disconnect from network"
#define QOS_OF_LAST_WILL      1
#define IS_LAST_WILL_RETAINED 1

#define ENCRYPT_CONNECTION     0
#define CERTIFICATE_INDEX      0
#define KEEP_ALIVE_INTERVAL    0
#define MQTT_CONNECT_TIMEOUT   1
#define MQTT_KEEPALIVE_RETRIES 0

#define SEND_CREDENTIALS 1

/******************************************************
 *               Variable Definitions
 ******************************************************/


const sl_wifi_device_configuration_t wifi_mqtt_client_configuration = {
    .boot_option = LOAD_NWP_FW,
    .mac_address = NULL,
    .band        = SL_SI91X_WIFI_BAND_2_4GHZ,
    .boot_config = { .oper_mode              = SL_SI91X_CLIENT_MODE,
        .coex_mode              = SL_SI91X_WLAN_ONLY_MODE,
        .feature_bit_map        = (SL_SI91X_FEAT_SECURITY_PSK | SL_SI91X_FEAT_AGGREGATION | SL_SI91X_FEAT_LONG_HTTP_URL),
        .tcp_ip_feature_bit_map = (SL_SI91X_TCP_IP_FEAT_DHCPV4_CLIENT
            | SL_SI91X_TCP_IP_FEAT_DNS_CLIENT
            | SL_SI91X_TCP_IP_FEAT_SSL
            | SL_SI91X_TCP_IP_FEAT_HTTP_CLIENT
            | SL_SI91X_TCP_IP_FEAT_EXTENSION_VALID
#ifdef SLI_SI91X_ENABLE_IPV6
            | SL_SI91X_TCP_IP_FEAT_DHCPV6_CLIENT | SL_SI91X_TCP_IP_FEAT_IPV6
#endif
        ),
        .custom_feature_bit_map     = ( SL_SI91X_CUSTOM_FEAT_EXTENTION_VALID),
        .ext_custom_feature_bit_map = (SL_SI91X_EXT_FEAT_SSL_VERSIONS_SUPPORT | SL_SI91X_EXT_FEAT_XTAL_CLK
            | SL_SI91X_EXT_FEAT_UART_SEL_FOR_DEBUG_PRINTS | MEMORY_CONFIG | SL_SI91X_EXT_FEAT_IEEE_80211W
#if defined(SLI_SI917) || defined(SLI_SI915)
            | SL_SI91X_EXT_FEAT_FRONT_END_SWITCH_PINS_ULP_GPIO_4_5_0
#endif
        ),
        .bt_feature_bit_map = 0,
        .ext_tcp_ip_feature_bit_map =
            (SL_SI91X_EXT_TCP_IP_WINDOW_SCALING | SL_SI91X_EXT_TCP_IP_TOTAL_SELECTS(10)
                | SL_SI91X_EXT_TCP_IP_FEAT_SSL_THREE_SOCKETS | SL_SI91X_EXT_TCP_IP_FEAT_SSL_MEMORY_CLOUD
                | SL_SI91X_EXT_EMB_MQTT_ENABLE | SL_SI91X_EXT_FEAT_HTTP_OTAF_SUPPORT),
                .ble_feature_bit_map     = 0,
                .ble_ext_feature_bit_map = 0,
                .config_feature_bit_map  = 0 }
};

static char MQTT_BROKER_IP[64];

static char USERNAME[32];

static char PASSWORD[32];

static bool sl_init_status = false;

static sl_mqtt_client_t client = { 0 };

static uint8_t is_execution_completed = 0;

static uint8_t is_mqtt_connected = 0;

static sl_mqtt_client_credentials_t *client_credentails = NULL;

sl_mqtt_client_configuration_t mqtt_client_configuration = { .is_clean_session = 1U,
    .client_id        = (uint8_t *)CLIENT_ID,
    .client_id_length = strlen(CLIENT_ID),
#if ENCRYPT_CONNECTION
.tls_flags = SL_MQTT_TLS_ENABLE | SL_MQTT_TLS_TLSV_1_2
| SL_MQTT_TLS_CERT_INDEX_1,
#endif
.client_port = CLIENT_PORT };

sl_mqtt_broker_v2_t mqtt_broker_configuration = {
    .port                    = MQTT_BROKER_PORT,
    .is_connection_encrypted = ENCRYPT_CONNECTION,
    .connect_timeout         = MQTT_CONNECT_TIMEOUT,
    .keep_alive_interval     = KEEP_ALIVE_INTERVAL,
    .keep_alive_retries      = MQTT_KEEPALIVE_RETRIES,
};

sl_mqtt_client_last_will_message_t last_will_message = {
    .is_retained         = IS_LAST_WILL_RETAINED,
    .will_qos_level      = QOS_OF_LAST_WILL,
    .will_topic          = (uint8_t *)LAST_WILL_TOPIC,
    .will_topic_length   = strlen(LAST_WILL_TOPIC),
    .will_message        = (uint8_t *)LAST_WILL_MESSAGE,
    .will_message_length = strlen(LAST_WILL_MESSAGE),
};

void mqtt_client_message_handler(void *client, sl_mqtt_client_message_t *message, void *context);
void mqtt_client_event_handler(void *client, sl_mqtt_client_event_t event, void *event_data, void *context);
void mqtt_client_error_event_handler(void *client, sl_mqtt_client_error_status_t *error);
void mqtt_client_cleanup();
void print_char_buffer(char *buffer, uint32_t buffer_length);

sl_status_t join_callback_handler(sl_wifi_event_t event, char *result, uint32_t result_length, void *arg)
{
  UNUSED_PARAMETER(result);
  UNUSED_PARAMETER(arg);

  printf("\r\nIn Join CB\r\n");

  if (SL_WIFI_CHECK_IF_EVENT_FAILED(event)) {
    printf("F: Initiating rejoin %lu bytes payload\n", result_length);
    return SL_STATUS_FAIL;
  }
  return SL_STATUS_OK;
}

void sl_net_error_to_text(sl_status_t status){
  switch(status)
  {
    case  SL_STATUS_INVALID_STATE:
      DEBUGOUT("SL_STATUS_INVALID_STATE:       Generic invalid state error.\n");
      break;
    case  SL_STATUS_NOT_READY:
      DEBUGOUT("SL_STATUS_NOT_READY:           Module is not ready for requested operation.\n");
      break;
    case  SL_STATUS_BUSY:
      DEBUGOUT("SL_STATUS_BUSY:                Module is busy and cannot carry out requested operation.\n");
      break;
    case  SL_STATUS_IN_PROGRESS:
      DEBUGOUT("SL_STATUS_IN_PROGRESS:         Operation is in progress and not yet complete (pass or fail).\n");
      break;
    case  SL_STATUS_ABORT:
      DEBUGOUT("SL_STATUS_ABORT:               Operation aborted.\n");
      break;
    case  SL_STATUS_TIMEOUT:
      DEBUGOUT("SL_STATUS_TIMEOUT:             Operation timed out.\n");
      break;
    case  SL_STATUS_PERMISSION:
      DEBUGOUT("SL_STATUS_PERMISSION:          Operation not allowed per permissions.\n");
      break;
    case  SL_STATUS_WOULD_BLOCK:
      DEBUGOUT("SL_STATUS_WOULD_BLOCK:         Non-blocking operation would block.\n");
      break;
    case  SL_STATUS_IDLE:
      DEBUGOUT("SL_STATUS_IDLE:                Operation/module is Idle, cannot carry requested operation.\n");
      break;
    case  SL_STATUS_IS_WAITING:
      DEBUGOUT("SL_STATUS_IS_WAITING:          Operation cannot be done while construct is waiting.\n");
      break;
    case  SL_STATUS_NONE_WAITING:
      DEBUGOUT("SL_STATUS_NONE_WAITING:        No task/construct waiting/pending for that action/event.\n");
      break;
    case  SL_STATUS_SUSPENDED:
      DEBUGOUT("SL_STATUS_SUSPENDED:           Operation cannot be done while construct is suspended.\n");
      break;
    case  SL_STATUS_NOT_AVAILABLE:
      DEBUGOUT("SL_STATUS_NOT_AVAILABLE:       Feature not available due to software configuration.\n");
      break;
    case  SL_STATUS_NOT_SUPPORTED:
      DEBUGOUT("SL_STATUS_NOT_SUPPORTED:       Feature not supported.\n");
      break;
    case  SL_STATUS_INITIALIZATION:
      DEBUGOUT("SL_STATUS_INITIALIZATION:      Initialization failed.\n");
      break;
    case  SL_STATUS_NOT_INITIALIZED:
      DEBUGOUT("SL_STATUS_NOT_INITIALIZED:     Module has not been initialized.\n");
      break;
    case  SL_STATUS_ALREADY_INITIALIZED:
      DEBUGOUT("SL_STATUS_ALREADY_INITIALIZED: Module has already been initialized.\n");
      break;
    case  SL_STATUS_DELETED:
      DEBUGOUT("SL_STATUS_DELETED:             Object/construct has been deleted.\n");
      break;
    case  SL_STATUS_ISR:
      DEBUGOUT("SL_STATUS_ISR:                 Illegal call from ISR.\n");
      break;
    case  SL_STATUS_NETWORK_UP:
      DEBUGOUT("SL_STATUS_NETWORK_UP:          Illegal call because network is up.\n");
      break;
    case  SL_STATUS_NETWORK_DOWN:
      DEBUGOUT("SL_STATUS_NETWORK_DOWN:        Illegal call because network is down.\n");
      break;
    case  SL_STATUS_NOT_JOINED:
      DEBUGOUT("SL_STATUS_NOT_JOINED:          Failure due to not being joined in a network.\n");
      break;
    case  SL_STATUS_NO_BEACONS:
      DEBUGOUT("SL_STATUS_NO_BEACONS:          Invalid operation as there are no beacons.\n");
      break;
  }
}

fm_comms_status fm_comms_init()
{
  DEBUGOUT("Initialising WiFi client interface.\n");
  sl_status_t status;

  // Deinitialise and initialise the WiFi subsystem to start clean
  sl_net_deinit(SL_NET_WIFI_CLIENT_INTERFACE);
  status = sl_net_init(SL_NET_WIFI_CLIENT_INTERFACE,
                       &wifi_mqtt_client_configuration,
                       NULL,
                       NULL);

  if (status != SL_STATUS_OK) {
      DEBUGOUT("Failed to start Wi-Fi client interface: 0x%lx\r\n", status);
      sl_net_error_to_text(status);
      return FMCOMMS_FAILED;
  }
  sl_init_status = true;
  return FMCOMMS_SUCCESS;
}

fm_comms_status fm_comms_connect()
{
  if(true != sl_init_status){
      return FMCOMMS_NOT_INITIALISED;
  }

  Ecode_t nvm_status;
  sl_status_t status;
  sl_net_wifi_client_profile_t profile;

  size_t num_objects = 0;
  status = nvm3_initDefault();
  if(status != SL_STATUS_OK){
      printf("Failed to initialise NVM: 0x%lx\r\n", status);
      return FMCOMMS_NVM_ERROR;
  }
  num_objects = nvm3_countObjects(nvm3_defaultHandle);
  char client_credential[64];
  if(num_objects < 5){
      printf("No objects in non-volatile memory\r\n");
      return FMCOMMS_NVM_EMPTPY;
  }
  printf("Found %d object(s) in non-volatile memory\r\n", num_objects);
  nvm_status = nvm3_readData(nvm3_defaultHandle,
                             NVM3_KEY_WIFI_PROFILE,
                             &profile,
                             sizeof(profile));
  if(nvm_status != ECODE_NVM3_OK){
      printf("failed to read profile from nvm: 0x%lx\r\n", nvm_status);
      return FMCOMMS_NVM_ERROR;
  }
  nvm_status = nvm3_readData(nvm3_defaultHandle,
                             NVM3_KEY_WIFI_CREDENTIAL,
                             &client_credential,
                             sizeof(client_credential));
  if(nvm_status != ECODE_NVM3_OK){
      printf("failed to read credentials from nvm: 0x%lx\r\n", nvm_status);
      return FMCOMMS_NVM_ERROR;
  }
  // Fetch the MQTT server address
  nvm_status = nvm3_readData(nvm3_defaultHandle,
                             NVM3_KEY_MQTT_ADDRESS,
                             MQTT_BROKER_IP,
                             sizeof(MQTT_BROKER_IP));
  if(nvm_status != ECODE_NVM3_OK){
      printf("failed to mqtt broker IP from nvm: 0x%lx\r\n", nvm_status);
      return FMCOMMS_NVM_ERROR;
  }
  // Fetch the MQTT client username
  nvm_status = nvm3_readData(nvm3_defaultHandle,
                             NVM3_KEY_MQTT_USERNAME,
                             USERNAME,
                             sizeof(USERNAME));
  if(nvm_status != ECODE_NVM3_OK){
      printf("failed to mqtt username from nvm: 0x%lx\r\n", nvm_status);
      return FMCOMMS_NVM_ERROR;
  }
  // Fetch the MQTT client password
  nvm_status = nvm3_readData(nvm3_defaultHandle,
                             NVM3_KEY_MQTT_PASSWORD,
                             PASSWORD,
                             sizeof(PASSWORD));
  if(nvm_status != ECODE_NVM3_OK){
      printf("failed to mqtt password from nvm: 0x%lx\r\n", nvm_status);
      return FMCOMMS_NVM_ERROR;
  }
  sl_wifi_credential_id_t id = 2; // TODO: make this a define
  sl_wifi_credential_t cred  = { 0 };

  cred.type = SL_WIFI_PSK_CREDENTIAL;
  memcpy(cred.psk.value, client_credential, strlen((char *)client_credential));
  status = sl_net_set_credential(id,
                                 SL_NET_WIFI_PSK,
                                 client_credential,
                                 strlen((char *)client_credential));
  if(status != SL_STATUS_OK){
      printf("failed to set wifi credential '%s': 0x%lx\r\n",
             client_credential,
             status);
  }

  if(status != SL_STATUS_OK){
      printf("Failed to fetch network profile %d. Error: 0x%lx\r\n",
             SL_NET_PROFILE_ID_1,
             status);
      return FMCOMMS_FAILED;
  }

  char ssid_buf[50];
  snprintf(ssid_buf,profile.config.ssid.length,"%s",profile.config.ssid.value);
  DEBUGOUT("Attempting to connect to '%s':'%s'\r\n",ssid_buf,client_credential);

  sl_wifi_set_join_callback(join_callback_handler, NULL);

  status = sl_wifi_connect(SL_WIFI_CLIENT_2_4GHZ_INTERFACE,
                           &profile.config,
                           18000);

  if (status == SL_STATUS_OK) {
      DEBUGOUT("WiFi connection success\r\n");
  } else if (status == SL_STATUS_SI91X_NO_AP_FOUND) {
      char ssid_name[32];
      snprintf(ssid_name,
               profile.config.ssid.length+1,
               "%s",
               profile.config.ssid.value);
      printf("WiFi network %s not found\r\n", ssid_name);
      return FMCOMMS_WIFI_NOT_FOUND;
  } else {
      printf("Failed to bring Wi-Fi client interface up: 0x%lx\r\n", status);
      sl_net_error_to_text(status);
      status = sl_net_deinit(SL_NET_WIFI_CLIENT_INTERFACE);
      return FMCOMMS_FAILED;
  }

  sl_ip_address_t ip_address            = { 0 };
  status = sl_si91x_configure_ip_address(&profile.ip, SL_SI91X_WIFI_CLIENT_VAP_ID);
  if (status != SL_STATUS_OK) {
      printf("IPv4 address configuration is failed : 0x%lx\r\n", status);
      return FMCOMMS_FAILED;
  }

  printf("IPv4 address configuration complete\r\n");
  memcpy(&ip_address.ip.v4.bytes, &profile.ip.ip.v4.ip_address.bytes, sizeof(sl_ipv4_address_t));
  printf("Client IPv4: ");
  print_sl_ipv4_address(&ip_address.ip.v4);
  printf("\r\n");

  sl_net_dns_address_t dns_address = {
      .primary_server_address = NULL,
      .secondary_server_address = NULL,
  };
  status = sl_net_set_dns_server(SL_NET_WIFI_CLIENT_INTERFACE, &dns_address);
  if(status != SL_STATUS_OK) {
      printf("Set DNS address failed : 0x%lx\r\n", status);
  }

  return FMCOMMS_SUCCESS;
}

void fm_comms_deint()
{
  if(true == sl_init_status){
    DEBUGOUT("Client is active. Disabling now\n");
    sl_status_t status;
    mqtt_client_cleanup();
    status = sl_wifi_disconnect(SL_WIFI_CLIENT_INTERFACE);
        if(status != SL_STATUS_OK) {
      printf("WiFi disconnect failed: 0x%lx\r\n",status);
    }

    status = sl_net_deinit((sl_net_interface_t)SL_NET_WIFI_CLIENT_INTERFACE);
    if(status != SL_STATUS_OK) {
      printf("WiFi denit failed: 0x%lx\r\n",status);
    }

    status = sl_net_deinit((sl_net_interface_t)SL_NET_WIFI_AP_INTERFACE);
    if(status != SL_STATUS_OK) {
      printf("AP deinit failed: 0x%lx\r\n",status);
    }

    sl_init_status = false;
  }
}

uint8_t fm_is_wifi_connected()
{
  uint8_t ret = 0U;
  if (true == sl_init_status){
    sl_wifi_interface_info_t info = {};
    sl_wifi_get_interface_info(SL_WIFI_CLIENT_INTERFACE, &info);
    ret = (uint8_t)info.wlan_state;
  }
  return ret;
}

uint8_t fm_is_mqtt_connected()
{
  return is_mqtt_connected;
}

fm_comms_status fm_comms_publish_data(const char * data, size_t len, const char * topic, size_t topic_len)
{
  if (SL_MQTT_CLIENT_CONNECTED != client.state){
      DEBUGOUT("Publish failed because client is disconnected:  %0xd\r\n", client.state);
      sl_mqtt_client_connect_v2(&client, NULL, NULL, NULL, 0);
      return FMCOMMS_FAILED;
  }

  sl_mqtt_client_message_t msg = {
      .content = (uint8_t*)data,
      .content_length = len,
      .is_duplicate_message = false,
      .is_retained = false,
      .qos_level = SL_MQTT_QOS_LEVEL_0,
      .topic = (uint8_t*)topic,
      .topic_length = topic_len,
  };
  uint32_t timeout_val_ms = 0;
  sl_status_t status = sl_mqtt_client_publish(&client, &msg, timeout_val_ms, NULL);
  if ((status != SL_STATUS_IN_PROGRESS) && (status != SL_STATUS_OK)) {
      DEBUGOUT("Failed to publish message: 0x%lx\r\n", status);
      return FMCOMMS_FAILED;
  }

  return FMCOMMS_SUCCESS;
}

fm_comms_status fm_comms_mqtt_start()
{
  // Only attempt MQTT connection if WiFi is active
  if(1U != fm_is_wifi_connected()){
      printf("Aborting MQTT connection. WiFi not found\r\n");
      return FMCOMMS_MQTT_ERROR;
  }

  sl_status_t status;

  if (ENCRYPT_CONNECTION) {
      // Load SSL CA certificate
      status = sl_net_set_credential(SL_NET_TLS_SERVER_CREDENTIAL_ID(CERTIFICATE_INDEX),
                                     SL_NET_SIGNING_CERTIFICATE,
                                     cacert,
                                     sizeof(cacert) - 1);
      if (status != SL_STATUS_OK) {
          DEBUGOUT("Loading TLS CA certificate in to FLASH Failed, Error Code : 0x%lX\r\n", status);
          return FMCOMMS_MQTT_ERROR;
      }
      DEBUGOUT("Load TLS CA certificate at index %d Success\r\n", 0);
  }

  if (SEND_CREDENTIALS) {
      uint16_t username_length, password_length;

      printf("Username: %s Password: %s \n",USERNAME, PASSWORD);

      username_length = strlen(USERNAME);
      password_length = strlen(PASSWORD);

      uint32_t malloc_size =
          sizeof(sl_mqtt_client_credentials_t) +
          username_length +
          password_length;

      client_credentails = malloc(malloc_size);
      if (client_credentails == NULL)
        return SL_STATUS_ALLOCATION_FAILED;
      memset(client_credentails, 0, malloc_size);
      client_credentails->username_length = username_length;
      client_credentails->password_length = password_length;

      memcpy(&client_credentails->data[0], USERNAME, username_length);
      memcpy(&client_credentails->data[username_length], PASSWORD, password_length);

      status = sl_net_set_credential(SL_NET_MQTT_CLIENT_CREDENTIAL_ID(0),
                                     SL_NET_MQTT_CLIENT_CREDENTIAL,
                                     client_credentails,
                                     malloc_size);

      if (status != SL_STATUS_OK) {
          mqtt_client_cleanup();
          DEBUGOUT("Failed to set credentials: 0x%lx\r\n ", status);

          return FMCOMMS_MQTT_ERROR;
      }
      DEBUGOUT("Set credentials Success \r\n ");

      free(client_credentails);
      mqtt_client_configuration.credential_id = SL_NET_MQTT_CLIENT_CREDENTIAL_ID(0);
  }

  status = sl_mqtt_client_init(&client, mqtt_client_event_handler);
  if (status != SL_STATUS_OK) {
      DEBUGOUT("Failed to initialise MQTT client: 0x%lx\r\n", status);

      mqtt_client_cleanup();
      return FMCOMMS_MQTT_ERROR;
  }
  DEBUGOUT("MQTT client initialised successfully \r\n");

#ifdef SLI_SI91X_ENABLE_IPV6
  unsigned char hex_addr[SL_IPV6_ADDRESS_LENGTH] = { 0 };
  status                                         = sl_inet_pton6(MQTT_BROKER_IP,
                                                                 MQTT_BROKER_IP + strlen(MQTT_BROKER_IP),
                                                                 hex_addr,
                                                                 (unsigned int *)mqtt_broker_configuration.ip.ip.v6.value);
  if (status != 0x1) {
      DEBUGOUT("\r\nIPv6 conversion failed.\r\n");
      mqtt_client_cleanup();
      return FMCOMMS_MQTT_ERROR;
  }
  mqtt_broker_configuration.ip.type = SL_IPV6;
#else
  sl_ip_address_t ip_addr;
  status = sl_net_dns_resolve_hostname(MQTT_BROKER_IP, 4000, SL_NET_DNS_TYPE_IPV4, &ip_addr);
  if (status == SL_STATUS_OK) {
      mqtt_broker_configuration.ip = ip_addr;
  } else {
    DEBUGOUT("Failed to resolve hostname %s with error 0x%lx. Attempting direct IP address connection...\n",
            MQTT_BROKER_IP, status);
    status = sl_net_inet_addr(MQTT_BROKER_IP, &mqtt_broker_configuration.ip.ip.v4.value);
    if (status != SL_STATUS_OK) {
        DEBUGOUT("Failed to convert IP address: 0x%lx \r\n", status);
        return FMCOMMS_FAILED; // return Failed so that we don't try to reconnect
                                // with broken IP address
    }
  }
  mqtt_broker_configuration.ip.type = SL_IPV4;
#endif

  status = sl_mqtt_client_connect_v2(&client,
                                  &mqtt_broker_configuration,
                                  &last_will_message,
                                  &mqtt_client_configuration,
                                  2000);
  osDelay(200U);
  DEBUGOUT("MQTT connection status: 0x%lx\r\n", status);
  if(status != SL_STATUS_OK) {
      DEBUGOUT("Failed to connect to mqtt broker: 0x%lx\r\n", status);
      return FMCOMMS_MQTT_ERROR;
  }
  if(client.state == SL_MQTT_CLIENT_CONNECTED){
      is_mqtt_connected = 1U;
  } else {
      is_mqtt_connected = 0U;
      return FMCOMMS_MQTT_ERROR;
  }

  DEBUGOUT("Connected to MQTT broker successfully \r\n");
  return FMCOMMS_SUCCESS;
}

bool fm_comms_mqtt_is_connected()
{
  if(client.state == SL_MQTT_CLIENT_DISCONNECTED)
    return false;
  else
    return true;
}

fm_comms_status fm_comms_mqtt_stop()
{
  fm_comms_status status = FMCOMMS_FAILED;
  if(fm_comms_mqtt_is_connected()){
    sl_status_t s = sl_mqtt_client_deinit(&client);
    if(s != SL_STATUS_FAIL)
      status = FMCOMMS_SUCCESS;
  }
  return status;
}

void mqtt_client_cleanup()
{
  is_mqtt_connected = 0U;
  sl_mqtt_client_disconnect(&client, 100);
  SL_CLEANUP_MALLOC(client_credentails);
  is_execution_completed = 1;
}

// Placeholder for future capability to receive MQTT messages
void mqtt_client_message_handler(void *client,
                                 sl_mqtt_client_message_t *message,
                                 void *context)
{
  UNUSED_PARAMETER(client);
  UNUSED_PARAMETER(context);

  DEBUGOUT("Message Received on Topic: ");

  print_char_buffer((char *)message->topic, message->topic_length);
  print_char_buffer((char *)message->content, message->content_length);
}

void print_char_buffer(char *buffer, uint32_t buffer_length)
{
  for (uint32_t index = 0; index < buffer_length; index++) {
      DEBUGOUT("%c", buffer[index]);
  }

  DEBUGOUT("\r\n");
}

void mqtt_client_error_event_handler(void *client_,
                                     sl_mqtt_client_error_status_t *error)
{
  (void)client_;
  DEBUGOUT("Terminating program, Error: %d\r\n", *error);
  is_mqtt_connected = 0U;
}

void mqtt_client_event_handler(void *client,
                               sl_mqtt_client_event_t event,
                               void *event_data,
                               void *context)
{
  UNUSED_PARAMETER(context);
  switch (event) {
    case SL_MQTT_CLIENT_CONNECTED_EVENT: {
      DEBUGOUT("Connected to broker\r\n");
      is_mqtt_connected = 1U;
      break;
    }

    case SL_MQTT_CLIENT_DISCONNECTED_EVENT: {
      DEBUGOUT("Disconnected from MQTT broker\r\n");
      is_mqtt_connected = 0U;
      break;
    }

    case SL_MQTT_CLIENT_ERROR_EVENT: {
      mqtt_client_error_event_handler(client,
                                      (sl_mqtt_client_error_status_t *)event_data);
      break;
    }
    default:
      break;
  }
}

