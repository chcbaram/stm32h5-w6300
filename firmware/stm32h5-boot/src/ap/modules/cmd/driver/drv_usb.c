#include "cmd_task.h"

#ifdef _USE_HW_CMD


//-- CDC 채널 드라이버.
//
//   cmd.c 는 전송계층과 무관하게 설계되어 있어서, open/close/available/read/write
//   여섯 개만 채워주면 같은 커맨드 셋이 그대로 동작한다. HID 채널(drv_hid.c)과
//   향후 이더넷(UDP)도 같은 방식으로 붙인다.
//
//   CDC 스트림은 CLI 와 공유하지 않는다. 호스트가 연 보율로 주인이 갈린다.
//   cmd_task.c 의 주석을 본다.
//
static bool drvUsbOpen(void *args)
{
  (void)args;
  return true;
}

static bool drvUsbClose(void *args)
{
  (void)args;
  return true;
}

static uint32_t drvUsbAvailable(void *args)
{
  (void)args;
  return cdcAvailable();
}

static bool drvUsbFlush(void *args)
{
  (void)args;
  while (cdcAvailable())
    cdcRead();
  return true;
}

static uint8_t drvUsbRead(void *args)
{
  (void)args;
  return cdcRead();
}

static uint32_t drvUsbWrite(void *args, uint8_t *p_data, uint32_t length)
{
  (void)args;
  return cdcWrite(p_data, length);
}


cmd_driver_t drv_usb_driver =
{
  .open      = drvUsbOpen,
  .close     = drvUsbClose,
  .available = drvUsbAvailable,
  .flush     = drvUsbFlush,
  .read      = drvUsbRead,
  .write     = drvUsbWrite,
};

#endif
