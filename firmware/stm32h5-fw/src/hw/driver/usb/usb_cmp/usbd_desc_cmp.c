/*
 * usbd_desc_cmp.c
 *
 *   composite(CDC + HID) 장치 디스크립터.
 *
 *   VID/PID 를 부트로더와 똑같이 쓴다. 호스트(웹/툴)는 하나의 필터로 두 상태를
 *   모두 잡고, 지금 붙은 쪽이 부트로더인지 앱인지는 BOOT_CMD_INFO 응답의
 *   HW_DEV_MODE 로 구분한다. USB 만으로 구분하려 들면 필터를 두 벌 관리해야 하고
 *   장치 선택창에도 두 항목이 뜬다.
 */

#include "usbd_core.h"
#include "usbd_desc_cmp.h"
#include "usbd_conf.h"
#include "hw_def.h"


#define USBD_VID                      HW_USB_VID
#define USBD_PID                      HW_USB_PID

#define USBD_LANGID_STRING            1033
#define USBD_MANUFACTURER_STRING      "BARAM"
#define USBD_PRODUCT_STRING           _DEF_BOARD_NAME
#define USBD_CONFIGURATION_STRING     "CMP Config"
#define USBD_INTERFACE_STRING         "CMP Interface"

//   시리얼 문자열 길이. 12바이트를 UTF-16 으로 펼치고 헤더 2바이트.
#ifndef USB_SIZ_STRING_SERIAL
#define USB_SIZ_STRING_SERIAL         0x1A
#endif

#define CMP_DEVICE_ID1                (UID_BASE)
#define CMP_DEVICE_ID2                (UID_BASE + 0x4)
#define CMP_DEVICE_ID3                (UID_BASE + 0x8)


static uint8_t *USBD_CMP_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CMP_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CMP_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CMP_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CMP_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CMP_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CMP_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

static void CmpGetSerialNum(void);
static void CmpIntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len);


USBD_DescriptorsTypeDef CMP_Desc =
{
  USBD_CMP_DeviceDescriptor,
  USBD_CMP_LangIDStrDescriptor,
  USBD_CMP_ManufacturerStrDescriptor,
  USBD_CMP_ProductStrDescriptor,
  USBD_CMP_SerialStrDescriptor,
  USBD_CMP_ConfigStrDescriptor,
  USBD_CMP_InterfaceStrDescriptor,
#if (USBD_CLASS_USER_STRING_DESC == 1)
  NULL,
#endif
};

//-- bDeviceClass = 0xEF / SubClass 0x02 / Protocol 0x01 (Miscellaneous, IAD).
//   CDC 가 인터페이스 2개를 IAD 로 묶기 때문에 반드시 이 조합이어야 한다.
//   0x00 으로 두면 윈도우가 첫 인터페이스만 보고 장치를 잘못 묶는다.
//
__ALIGN_BEGIN static uint8_t USBD_CMP_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END =
{
  0x12,                       /* bLength */
  USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
  0x00, 0x02,                 /* bcdUSB 2.00 */
  0xEF,                       /* bDeviceClass: Miscellaneous */
  0x02,                       /* bDeviceSubClass: Common Class */
  0x01,                       /* bDeviceProtocol: IAD */
  USB_MAX_EP0_SIZE,           /* bMaxPacketSize0 */
  LOBYTE(USBD_VID), HIBYTE(USBD_VID),
  LOBYTE(USBD_PID), HIBYTE(USBD_PID),
  0x00, 0x02,                 /* bcdDevice 2.00 */
  USBD_IDX_MFC_STR,
  USBD_IDX_PRODUCT_STR,
  USBD_IDX_SERIAL_STR,
  USBD_MAX_NUM_CONFIGURATION
};

__ALIGN_BEGIN static uint8_t USBD_CMP_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END =
{
  USB_LEN_LANGID_STR_DESC,
  USB_DESC_TYPE_STRING,
  LOBYTE(USBD_LANGID_STRING),
  HIBYTE(USBD_LANGID_STRING)
};

__ALIGN_BEGIN static uint8_t USBD_CMP_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

__ALIGN_BEGIN static uint8_t USBD_CMP_StringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END =
{
  USB_SIZ_STRING_SERIAL,
  USB_DESC_TYPE_STRING,
};


static uint8_t *USBD_CMP_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(USBD_CMP_DeviceDesc);
  return USBD_CMP_DeviceDesc;
}

static uint8_t *USBD_CMP_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = sizeof(USBD_CMP_LangIDDesc);
  return USBD_CMP_LangIDDesc;
}

static uint8_t *USBD_CMP_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_CMP_StrDesc, length);
  return USBD_CMP_StrDesc;
}

static uint8_t *USBD_CMP_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)USBD_PRODUCT_STRING, USBD_CMP_StrDesc, length);
  return USBD_CMP_StrDesc;
}

static uint8_t *USBD_CMP_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  *length = USB_SIZ_STRING_SERIAL;
  CmpGetSerialNum();
  return USBD_CMP_StringSerial;
}

static uint8_t *USBD_CMP_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING, USBD_CMP_StrDesc, length);
  return USBD_CMP_StrDesc;
}

static uint8_t *USBD_CMP_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  UNUSED(speed);
  USBD_GetString((uint8_t *)USBD_INTERFACE_STRING, USBD_CMP_StrDesc, length);
  return USBD_CMP_StrDesc;
}

static void CmpGetSerialNum(void)
{
  uint32_t deviceserial0, deviceserial1, deviceserial2;

  deviceserial0 = *(uint32_t *)CMP_DEVICE_ID1;
  deviceserial1 = *(uint32_t *)CMP_DEVICE_ID2;
  deviceserial2 = *(uint32_t *)CMP_DEVICE_ID3;

  deviceserial0 += deviceserial2;

  if (deviceserial0 != 0)
  {
    CmpIntToUnicode(deviceserial0, &USBD_CMP_StringSerial[2], 8);
    CmpIntToUnicode(deviceserial1, &USBD_CMP_StringSerial[18], 4);
  }
}

static void CmpIntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
  uint8_t idx;

  for (idx = 0; idx < len; idx++)
  {
    if (((value >> 28)) < 0xA)
    {
      pbuf[2 * idx] = (uint8_t)((value >> 28) + '0');
    }
    else
    {
      pbuf[2 * idx] = (uint8_t)((value >> 28) + 'A' - 10);
    }

    value = value << 4;
    pbuf[2 * idx + 1] = 0;
  }
}
