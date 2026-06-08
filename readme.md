# FridgeMonitor V2

## Table of Contents

- [FridgeMonitor V2](#fridgemontior-v2)
  - [Table of Contents](#table-of-contents)
  - [Purpose/Scope](#purposescope)
  - [Prerequisites/Setup Requirements](#prerequisitessetup-requirements)
    - [Hardware Requirements](#hardware-requirements)
    - [Software Requirements](#software-requirements)
    - [Setup Diagram](#setup-diagram)
  - [Getting Started](#getting-started)
  - [Application Build Environment](#application-build-environment)
  - [Test the Application](#test-the-application)
  
## Purpose/Scope
This application reads sensors using on-board ADC channels and publishes them to a server using MQTT over WiFi.

## Prerequisites/Setup Requirements
Make the following modifications to the Wiseconnect SDK, until it is patched.
In sl_si91x_adc.h, add the following declaration
```c
sl_status_t sl_si91x_adc_read_data_static_v2(sl_adc_channel_config_t* adc_channel_config,
                                          sl_adc_config_t adc_config,
                                          uint16_t *adc_value);
```

In sl_si91x_adc.c, add the following definition
```c
sl_status_t sl_si91x_adc_read_data_static_v2(sl_adc_channel_config_t* adc_channel_config,
                                          sl_adc_config_t adc_config,
                                          uint16_t *adc_value)
{
  sl_status_t status;
  rsi_error_t error_status;
  uint8_t data_process     = 1;
  static uint8_t chnl_num  = 0;
  int16_t read_static_data = 0;
  do {
    // Validate ADC parameters, if the parameters are incorrect
    // If the status is not equal to SL_STATUS_OK, returns error code.
    status = validate_adc_channel_parameters(adc_channel_config);
    if (status != SL_STATUS_OK) {
      break;
    }
    // Enable the gain and calculation on output samples.
    read_static_data = RSI_ADC_ReadDataStatic(AUX_ADC_DAC_COMP, data_process, adc_channel_config->input_type[chnl_num]);
    *adc_value       = (uint16_t)read_static_data;
    if (adc_config.num_of_channel_enable == 1) {
      // If adc using only one channel then it will clear the interrupt to sample the next data.
      status = sl_si91x_adc_channel_interrupt_clear(adc_config, chnl_num);
    } else { // If number of channel more than one it will reconfig the next channel and it will sample the data in a sequential order.
      if (++chnl_num >= adc_config.num_of_channel_enable) {
        chnl_num = 0;
      }
      adc_channel_config->channel = chnl_num;
      error_status               = ADC_Per_ChannelConfig(*adc_channel_config, adc_config);
      status                     = convert_rsi_to_sl_error_code(error_status);
    }
  } while (false);
  return status;
}
```

In si91x\mcu\drivers\peripheral_drivers\src\rsi_adc.c, modify the function `check_power_two` as follows:
```c
uint8_t check_power_two(uint16_t num)
{
  // 0 is not a power of two
  if (num == 0) {
    return 0;
  }
  
  while (num != 1) {
    if (num % 2) {
      return 0;
    }
    num /= 2;
  }
  return 1;
}
```



### Hardware Requirements

## Application Build Environment


## Test the Application

Refer to the instructions [here]()

- Build the application.
- Flash, run, and debug the application.
- After the application gets executed successfully
