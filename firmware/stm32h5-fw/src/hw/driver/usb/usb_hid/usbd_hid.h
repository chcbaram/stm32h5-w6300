/*
 * usbd_hid.h
 *
 *   vendor-defined HID 클래스. 키보드/마우스가 아니라 64바이트 IN/OUT 리포트를
 *   주고받는 통신 채널이다. 부트로더(TinyUSB)의 HID 인터페이스와 리포트
 *   디스크립터가 바이트 단위로 같아야 웹 업데이터가 앱/부트로더를 가리지 않고
 *   같은 필터로 잡을 수 있다.
 */

#ifndef __USB_HID_H
#define __USB_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_ioreq.h"


//-- 엔드포인트 배정
//   CDC 가 EP1(데이터) 과 EP2(notification) 를 쓰므로 HID 는 EP3 를 쓴다.
//   composite 모드에서는 usb.c 의 hid_ep_tbl[] 이 실제 주소를 넘겨주고,
//   usbd_hid.c 는 USBD_CoreGetEPAdd() 로 되찾아간다.
//
#define HID_EPIN_ADDR                   0x83U
#define HID_EPOUT_ADDR                  0x03U
#define HID_EP_SIZE                     64U

#define HID_FS_BINTERVAL                0x01U
#define HID_HS_BINTERVAL                0x01U

#define HID_DESCRIPTOR_TYPE             0x21U
#define HID_REPORT_DESC                 0x22U
#define HID_REPORT_DESC_SIZE            34U

#define USB_HID_DESC_SIZ                9U
#define USB_HID_CONFIG_DESC_SIZ         41U

#define USBD_HID_REQ_SET_PROTOCOL       0x0BU
#define USBD_HID_REQ_GET_PROTOCOL       0x03U
#define USBD_HID_REQ_SET_IDLE           0x0AU
#define USBD_HID_REQ_GET_IDLE           0x02U
#define USBD_HID_REQ_SET_REPORT         0x09U
#define USBD_HID_REQ_GET_REPORT         0x01U


typedef enum
{
  USBD_HID_IDLE = 0,
  USBD_HID_BUSY,
} USBD_HID_StateTypeDef;

typedef struct
{
  uint32_t              Protocol;
  uint32_t              IdleState;
  uint32_t              AltSetting;
  USBD_HID_StateTypeDef state;
  uint8_t               RxBuffer[HID_EP_SIZE];
} USBD_HID_HandleTypeDef;

/*
 * HID Class specification version 1.1
 * 6.2.1 HID Descriptor
 */
typedef struct
{
  uint8_t   bLength;
  uint8_t   bDescriptorType;
  uint16_t  bcdHID;
  uint8_t   bCountryCode;
  uint8_t   bNumDescriptors;
  uint8_t   bHIDDescriptorType;
  uint16_t  wItemLength;
} __PACKED USBD_HIDDescTypeDef;

//-- 애플리케이션 콜백.
//   Receive() 는 USB 인터럽트 문맥에서 불린다. 링버퍼에 넣기만 할 것.
//
typedef struct
{
  int8_t (*Init)(void);
  int8_t (*DeInit)(void);
  int8_t (*Receive)(uint8_t *p_buf, uint32_t length);
} USBD_HID_ItfTypeDef;


extern USBD_ClassTypeDef USBD_HID;
#define USBD_HID_CLASS &USBD_HID


uint8_t USBD_HID_RegisterInterface(USBD_HandleTypeDef *pdev, USBD_HID_ItfTypeDef *fops);

#ifdef USE_USBD_COMPOSITE
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len, uint8_t ClassId);
uint8_t USBD_HID_IsReady(USBD_HandleTypeDef *pdev, uint8_t ClassId);
#else
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len);
uint8_t USBD_HID_IsReady(USBD_HandleTypeDef *pdev);
#endif

uint32_t USBD_HID_GetPollingInterval(USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif /* __USB_HID_H */
