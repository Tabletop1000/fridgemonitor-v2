/***************************************************************************/ /**
 * @file app.h
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef APP_AP_SEL_H
#define APP_AP_SEL_H

/***************************************************************************/ /**
 * Initialize application.
 ******************************************************************************/
void fm_ap_sel_start(void);
unsigned char is_wifi_connected(void);

/***************************************************************************/ /**
 * App ticking function.
 ******************************************************************************/
void app_ap_sel_process_action(void);

#endif // APP_H
