#include "cmd_task.h"

#ifdef _USE_HW_CMD


//-- CDC 채널 드라이버.
//
//   cmd.c 는 전송계층과 무관하게 설계되어 있어서, open/close/available/read/write
//   여섯 개만 채워주면 같은 커맨드 셋이 그대로 동작한다. HID 채널(cmd_hid.c)과
//   향후 이더넷(UDP)도 같은 방식으로 붙인다.
//
static bool cmdUsbOpen(void *args)
{
  (void)args;
  return true;
}

static bool cmdUsbClose(void *args)
{
  (void)args;
  return true;
}

static uint32_t cmdUsbAvailable(void *args)
{
  (void)args;
  return cdcAvailable();
}

static bool cmdUsbFlush(void *args)
{
  (void)args;
  while (cdcAvailable())
    cdcRead();
  return true;
}

static uint8_t cmdUsbRead(void *args)
{
  (void)args;
  return cdcRead();
}

static uint32_t cmdUsbWrite(void *args, uint8_t *p_data, uint32_t length)
{
  (void)args;
  return cdcWrite(p_data, length);
}


cmd_driver_t cmd_usb_driver =
{
  .open      = cmdUsbOpen,
  .close     = cmdUsbClose,
  .available = cmdUsbAvailable,
  .flush     = cmdUsbFlush,
  .read      = cmdUsbRead,
  .write     = cmdUsbWrite,
};

#endif
