/**
  ******************************************************************************
  * @file    USB_Device/CDC_Standalone/Inc/usbd_conf.h
  * @author  MCD Application Team
  * @brief   General low level driver configuration
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CONF_H
#define __USBD_CONF_H

/* Includes ------------------------------------------------------------------*/
#include "bsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Common Config */
//-- composite (CDC 2 + HID 1 = 인터페이스 3개, 클래스 2개)
//
//   USBD_MAX_NUM_INTERFACES 가 실제 인터페이스 수보다 작으면 빌더의
//   USBD_CMPSIT_FindFreeIFNbr() 이 빈 슬롯을 못 찾아 인터페이스 번호가 겹친다.
//   증상이 "열거는 되는데 HID 만 안 보인다" 로 나와 원인을 찾기 어렵다.
//
#define USE_USBD_COMPOSITE
#define USBD_MAX_SUPPORTED_CLASS              2U
#define USBD_MAX_CLASS_INTERFACES             3U
#define USBD_MAX_CLASS_ENDPOINTS              3U
#define USBD_CMPST_MAX_CONFDESC_SZ            200U

//   CDC 의 Comm/Data 인터페이스를 IAD 로 묶는다. 장치 디스크립터를
//   0xEF/0x02/0x01 (Miscellaneous, IAD) 로 선언해 놓고 IAD 를 빼면 윈도우가
//   두 인터페이스를 별개 장치로 보고 VCP 를 못 만든다. 빌더는 이 매크로가
//   1 일 때만 IAD 를 붙인다(기본값이 없어 정의하지 않으면 0 취급).
#define USBD_COMPOSITE_USE_IAD                1

#define USBD_MAX_NUM_INTERFACES               3
#define USBD_MAX_NUM_CONFIGURATION            1
#define USBD_MAX_STR_DESC_SIZ                 0x100
#define USBD_SUPPORT_USER_STRING              0 
#define USBD_SELF_POWERED                     1
#define USBD_LPM_ENABLED                      0
#define USBD_DEBUG_LEVEL                      0


#define DEVICE_FS 		0
#define DEVICE_HS 		1

/* Exported macro ------------------------------------------------------------*/
/* Memory management macros */   
#define USBD_malloc               (void *)USBD_static_malloc
#define USBD_free                 USBD_static_free
#define USBD_memset               memset
#define USBD_memcpy               memcpy
#define USBD_Delay                HAL_Delay

/* DEBUG macros */  
#if (USBD_DEBUG_LEVEL > 0)
#define  USBD_UsrLog(...)   printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBD_UsrLog(...)   
#endif                            
                            
#if (USBD_DEBUG_LEVEL > 1)

#define  USBD_ErrLog(...)   printf("ERROR: ") ;\
                            printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBD_ErrLog(...)   
#endif 
                                                        
#if (USBD_DEBUG_LEVEL > 2)                         
#define  USBD_DbgLog(...)   printf("DEBUG : ") ;\
                            printf(__VA_ARGS__);\
                            printf("\n");
#else
#define USBD_DbgLog(...)                         
#endif

/* Exported functions ------------------------------------------------------- */
void *USBD_static_malloc(uint32_t size);
void USBD_static_reset(void);
void USBD_static_free(void *p);

bool USBD_is_connected(void);

#endif /* __USBD_CONF_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
