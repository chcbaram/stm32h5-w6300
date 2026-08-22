#include "hid.h"

#if defined(_USE_HW_USB) && (HW_USE_HID == 1)

#include "usb.h"
#include "usbd_hid.h"


//-- OUT 리포트 수신자. 실제 처리는 ap/modules/cmd/driver/drv_hid.c 가 한다.
//   USB 인터럽트 문맥이라 여기서는 넘겨주기만 한다.
//
#ifdef _USE_HW_CMD
extern void drvHidRxReport(uint8_t const *p_buf, uint16_t length);
#endif

extern USBD_HandleTypeDef USBD_Device;

static int8_t hidItfInit(void);
static int8_t hidItfDeInit(void);
static int8_t hidItfReceive(uint8_t *p_buf, uint32_t length);

static bool is_init = false;

USBD_HID_ItfTypeDef USBD_HID_fops =
{
  hidItfInit,
  hidItfDeInit,
  hidItfReceive,
};




bool hidInit(void)
{
  is_init = true;
  return true;
}

bool hidIsConnect(void)
{
  if (is_init != true)
    return false;

  return usbIsConnect();
}

bool hidIsReady(void)
{
  if (is_init != true)
    return false;

#ifdef USE_USBD_COMPOSITE
  return USBD_HID_IsReady(&USBD_Device, usbGetHidClassId()) == 1U;
#else
  return USBD_HID_IsReady(&USBD_Device) == 1U;
#endif
}

//-- 리포트 한 장(64바이트)을 보낸다.
//   IN 엔드포인트가 비어 있지 않으면 0 을 돌려준다. 재시도는 호출자 몫이다.
//
uint32_t hidWrite(uint8_t *p_data, uint32_t length)
{
  uint8_t ret;

  if (is_init != true)
    return 0;

  if (length > HID_PACKET_SIZE)
    length = HID_PACKET_SIZE;

#ifdef USE_USBD_COMPOSITE
  ret = USBD_HID_SendReport(&USBD_Device, p_data, (uint16_t)length, usbGetHidClassId());
#else
  ret = USBD_HID_SendReport(&USBD_Device, p_data, (uint16_t)length);
#endif

  return (ret == (uint8_t)USBD_OK) ? length : 0;
}

static int8_t hidItfInit(void)
{
  return (int8_t)USBD_OK;
}

static int8_t hidItfDeInit(void)
{
  return (int8_t)USBD_OK;
}

static int8_t hidItfReceive(uint8_t *p_buf, uint32_t length)
{
#ifdef _USE_HW_CMD
  drvHidRxReport(p_buf, (uint16_t)length);
#else
  (void)p_buf;
  (void)length;
#endif
  return (int8_t)USBD_OK;
}

#endif
