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
  { "USB CDC", {0}, &drv_usb_driver },
#if defined(HW_USE_HID) && HW_USE_HID == 1
  { "USB HID", {0}, &drv_hid_driver },
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

  drvCliInit();

  logPrintf("[OK] cmdTaskInit()\n");
  for (uint32_t i = 0; i < CMD_CH_MAX; i++)
    logPrintf("     %s\n", cmd_ch[i].name);

  return true;
}

//-- CDC 는 CLI 와 한 스트림을 나눠 쓸 수 없다.
//
//   둘 다 cdcRead() 를 부르면 서로 바이트를 훔쳐 양쪽 다 깨진다. cmd.c 는 STX0 이
//   아닌 바이트를 소비하고 버리므로, "먼저 읽는 쪽이 이긴다" 를 조정하는 것으로는
//   풀리지 않는다.
//
//   그래서 호스트가 연 **보율로 주인을 가른다**. 다른 보드에서 쓰던 방식과 같다.
//
//     115200  -> USB_CON_CLI. 터미널이다. CLI 가 CDC 를 쥐고 cmd 는 물러난다
//     그 외    -> USB_CON_CDC. 호스트 툴이다. cmd 가 CDC 를 독점한다
//
//   HID 는 전용 채널이라 이 판정과 무관하게 항상 돈다.
//
static bool cmdChIsEnabled(uint32_t index)
{
#if defined(_USE_HW_CDC) && HW_USE_CDC == 1
  if (cmd_ch[index].p_driver == &drv_usb_driver)
  {
    if (cdcIsConnect() == true && usbGetType() == USB_CON_CLI)
      return false;
  }
#else
  (void)index;
#endif
  return true;
}

bool cmdTaskUpdate(void)
{
  //-- 재진입 방지.
  //
  //   커맨드 처리 중의 delay() 는 cliLoopIdle() -> moduleUpdate() 를 부르고,
  //   그 안에 이 함수가 다시 들어온다. 그대로 두면 처리 도중에 다음 패킷을
  //   또 처리해서 응답이 뒤섞인다(CLI 출력 버퍼는 하나뿐이다).
  //
  //   다만 drvCliUpdate() 는 **반드시 통과**시켜야 한다. cliMain() 안에 갇혀
  //   있는 반복 명령을 끊어주는 것이 바로 이 경로이기 때문이다.
  //
  static bool is_busy = false;

  drvCliUpdate();

  if (is_busy)
    return false;

  is_busy = true;
  for (uint32_t i = 0; i < CMD_CH_MAX; i++)
  {
    if (cmdChIsEnabled(i) != true)
      continue;

    if (cmdReceivePacket(&cmd_ch[i].cmd) == true)
    {
      cmdBootProcess(&cmd_ch[i].cmd);
    }
  }
  is_busy = false;

  return true;
}

#endif
