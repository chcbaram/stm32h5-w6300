#include "cmd_task.h"

#ifdef _USE_HW_CMD


//-- 채널마다 cmd_t 인스턴스를 하나씩 둔다.
//   패킷 파서 상태가 채널별로 독립이어야 하기 때문이다.
//
typedef struct
{
  const char   *name;
  cmd_t         cmd;
  cmd_driver_t *p_driver;
} cmd_ch_t;

static cmd_ch_t cmd_ch[] =
{
  { "USB CDC", {0}, &cmd_usb_driver },
#if defined(HW_USE_HID) && HW_USE_HID == 1
  { "USB HID", {0}, &cmd_hid_driver },
#endif
};

#define CMD_CH_MAX  (sizeof(cmd_ch)/sizeof(cmd_ch[0]))



bool cmdTaskInit(void)
{
  for (uint32_t i = 0; i < CMD_CH_MAX; i++)
  {
    cmdInit(&cmd_ch[i].cmd, cmd_ch[i].p_driver);
    cmdOpen(&cmd_ch[i].cmd);
  }

  cliCmdInit();

  logPrintf("[OK] cmdTaskInit()\n");
  for (uint32_t i = 0; i < CMD_CH_MAX; i++)
    logPrintf("     %s\n", cmd_ch[i].name);

  return true;
}

bool cmdTaskUpdate(void)
{
  for (uint32_t i = 0; i < CMD_CH_MAX; i++)
  {
    if (cmdReceivePacket(&cmd_ch[i].cmd) == true)
    {
      cmdBootProcess(&cmd_ch[i].cmd);
    }
  }
  return true;
}

#endif
