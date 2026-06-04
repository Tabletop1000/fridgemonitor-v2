#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// $[USART0]
// [USART0]$

// $[UART1]
// [UART1]$

// $[ULP_UART]
// [ULP_UART]$

// $[I2C0]
// [I2C0]$

// $[I2C1]
// [I2C1]$

// $[ULP_I2C]
// [ULP_I2C]$

// $[SSI_MASTER]
// [SSI_MASTER]$

// $[SSI_SLAVE]
// [SSI_SLAVE]$

// $[ULP_SSI]
// [ULP_SSI]$

// $[GSPI_MASTER]
// [GSPI_MASTER]$

// $[I2S0]
// [I2S0]$

// $[ULP_I2S]
// [ULP_I2S]$

// $[SCT]
// [SCT]$

// $[SIO]
// [SIO]$

// $[PWM]
// [PWM]$

// $[PWM_CH0]
// [PWM_CH0]$

// $[PWM_CH1]
// [PWM_CH1]$

// $[PWM_CH2]
// [PWM_CH2]$

// $[PWM_CH3]
// [PWM_CH3]$

// $[ADC_CH1]
// ADC_CH1 P on ULP_GPIO_1/GPIO_65
#ifndef ADC_CH1_P_PORT                          
#define ADC_CH1_P_PORT                           ULP
#endif
#ifndef ADC_CH1_P_PIN                           
#define ADC_CH1_P_PIN                            1
#endif
#ifndef ADC_CH1_P_LOC                           
#define ADC_CH1_P_LOC                            10
#endif

// [ADC_CH1]$

// $[ADC_CH2]
// ADC_CH2 P on GPIO_27
#ifndef ADC_CH2_P_PORT                          
#define ADC_CH2_P_PORT                           HP
#endif
#ifndef ADC_CH2_P_PIN                           
#define ADC_CH2_P_PIN                            27
#endif
#ifndef ADC_CH2_P_LOC                           
#define ADC_CH2_P_LOC                            26
#endif

// [ADC_CH2]$

// $[ADC_CH3]
// ADC_CH3 P on GPIO_28
#ifndef ADC_CH3_P_PORT                          
#define ADC_CH3_P_PORT                           HP
#endif
#ifndef ADC_CH3_P_PIN                           
#define ADC_CH3_P_PIN                            28
#endif
#ifndef ADC_CH3_P_LOC                           
#define ADC_CH3_P_LOC                            55
#endif

// [ADC_CH3]$

// $[ADC_CH4]
// ADC_CH4 P on GPIO_25
#ifndef ADC_CH4_P_PORT                          
#define ADC_CH4_P_PORT                           HP
#endif
#ifndef ADC_CH4_P_PIN                           
#define ADC_CH4_P_PIN                            25
#endif
#ifndef ADC_CH4_P_LOC                           
#define ADC_CH4_P_LOC                            63
#endif

// [ADC_CH4]$

// $[ADC_CH5]
// ADC_CH5 P on GPIO_26
#ifndef ADC_CH5_P_PORT                          
#define ADC_CH5_P_PORT                           HP
#endif
#ifndef ADC_CH5_P_PIN                           
#define ADC_CH5_P_PIN                            26
#endif
#ifndef ADC_CH5_P_LOC                           
#define ADC_CH5_P_LOC                            92
#endif

// [ADC_CH5]$

// $[ADC_CH6]
// ADC_CH6 P on GPIO_29
#ifndef ADC_CH6_P_PORT                          
#define ADC_CH6_P_PORT                           HP
#endif
#ifndef ADC_CH6_P_PIN                           
#define ADC_CH6_P_PIN                            29
#endif
#ifndef ADC_CH6_P_LOC                           
#define ADC_CH6_P_LOC                            103
#endif

// [ADC_CH6]$

// $[ADC_CH7]
// ADC_CH7 P on GPIO_30
#ifndef ADC_CH7_P_PORT                          
#define ADC_CH7_P_PORT                           HP
#endif
#ifndef ADC_CH7_P_PIN                           
#define ADC_CH7_P_PIN                            30
#endif
#ifndef ADC_CH7_P_LOC                           
#define ADC_CH7_P_LOC                            132
#endif

// [ADC_CH7]$

// $[ADC_CH8]
// [ADC_CH8]$

// $[ADC_CH9]
// [ADC_CH9]$

// $[ADC_CH10]
// [ADC_CH10]$

// $[ADC_CH11]
// [ADC_CH11]$

// $[ADC_CH12]
// [ADC_CH12]$

// $[ADC_CH13]
// [ADC_CH13]$

// $[ADC_CH14]
// [ADC_CH14]$

// $[ADC_CH15]
// [ADC_CH15]$

// $[ADC_CH16]
// [ADC_CH16]$

// $[ADC_CH17]
// [ADC_CH17]$

// $[ADC_CH18]
// [ADC_CH18]$

// $[ADC_CH19]
// [ADC_CH19]$

// $[COMP1]
// [COMP1]$

// $[COMP2]
// [COMP2]$

// $[DAC0]
// [DAC0]$

// $[DAC1]
// [DAC1]$

// $[SYSRTC]
// [SYSRTC]$

// $[UULP_VBAT_GPIO]
// [UULP_VBAT_GPIO]$

// $[GPIO]
// [GPIO]$

// $[QEI]
// [QEI]$

// $[SDIO]
// [SDIO]$

// $[HSPI_SECONDARY]
// [HSPI_SECONDARY]$

// $[MCU_CLK_OUT]
// [MCU_CLK_OUT]$

// $[CUSTOM_PIN_NAME]
#ifndef Thermistor_1_PORT                       
#define Thermistor_1_PORT                        HP
#endif
#ifndef Thermistor_1_PIN                        
#define Thermistor_1_PIN                         25
#endif

#ifndef Thermistor_2_PORT                       
#define Thermistor_2_PORT                        HP
#endif
#ifndef Thermistor_2_PIN                        
#define Thermistor_2_PIN                         26
#endif

#ifndef Thermistor_3_PORT                       
#define Thermistor_3_PORT                        HP
#endif
#ifndef Thermistor_3_PIN                        
#define Thermistor_3_PIN                         27
#endif

#ifndef Thermistor_4_PORT                       
#define Thermistor_4_PORT                        HP
#endif
#ifndef Thermistor_4_PIN                        
#define Thermistor_4_PIN                         28
#endif

#ifndef Pressure_1_PORT                         
#define Pressure_1_PORT                          HP
#endif
#ifndef Pressure_1_PIN                          
#define Pressure_1_PIN                           29
#endif

#ifndef Pressure_2_PORT                         
#define Pressure_2_PORT                          HP
#endif
#ifndef Pressure_2_PIN                          
#define Pressure_2_PIN                           30
#endif

#ifndef Power_PORT                              
#define Power_PORT                               ULP
#endif
#ifndef Power_PIN                               
#define Power_PIN                                1
#endif

// [CUSTOM_PIN_NAME]$

#endif // PIN_CONFIG_H

// $[SDC_CH1]
// [SDC_CH1]$

// $[SDC_CH2]
// [SDC_CH2]$

// $[SDC_CH3]
// [SDC_CH3]$

// $[SDC_CH4]
// [SDC_CH4]$

// $[OPAMP1]
// [OPAMP1]$

// $[OPAMP2]
// [OPAMP2]$

// $[OPAMP3]
// [OPAMP3]$

