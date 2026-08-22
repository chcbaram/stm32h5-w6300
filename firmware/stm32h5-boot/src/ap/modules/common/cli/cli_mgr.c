#include "cli_mgr.h"

#ifdef _USE_HW_WIZNET
#include "driver/cli_net.h"
#endif

#ifdef _USE_HW_CLI


static uint8_t  cli_ch    = HW_UART_CH_CLI;
static uint32_t cli_baud  = 115200;
static bool     is_enable = true;



bool cliMgrInit(void)
{
#ifdef _USE_HW_WIZNET
  cliNetInit(23);
#endif

  cliOpen(cli_ch, cli_baud);
  cliBegin();
  return true;
}

void cliMgrEnable(bool enable)
{
  is_enable = enable;
}

void cliMgrThread(void const *arg)
{
  UNUSED(arg);

  //-- 비활성 구간에서는 cliMain() 뿐 아니라 **포트 자동 전환도** 멈춘다.
  //
  //   cmd 채널의 CLI(BOOT_CMD_CLI)는 명령을 실행하는 동안 CLI 포트를 가상
  //   채널(HW_UART_CH_CMD)로 돌려놓는다. 그 사이 이 함수가 돌면서 포트를
  //   원래대로 되돌려 버리면, cliKeepLoop() 이 엉뚱한 포트의 입력을 보게 되어
  //   `usb info` 같은 반복 명령이 영원히 빠져나오지 못한다.
  //
  //   이 함수는 delay() -> cliLoopIdle() -> moduleUpdate() 로 재진입하므로
  //   명령 실행 중에도 계속 불린다. 아래 전환 로직까지 함께 막아야 한다.
  //
  if (is_enable != true)
  {
    return;
  }

  cliMain();

#ifdef _USE_HW_WIZNET
  cliNetPoll();
  if (cliNetIsConnected())
  {
    cli_ch = HW_UART_CH_NET;
  }
  else if (cli_ch == HW_UART_CH_NET)
  {
    cli_ch = HW_UART_CH_CLI;
  }
#endif

  //-- CDC 는 CLI 와 cmd 패킷이 한 스트림을 나눠 쓸 수 없다.
  //
  //   호스트가 연 보율로 주인을 가른다. 115200 으로 열면 터미널로 보고 CLI 가
  //   가져가고, 그 외 보율이면 호스트 툴로 보고 cmd 에 넘긴다
  //   (ap/modules/cmd/cmd_task.c 의 cmdChIsEnabled()).
  //
#ifdef _USE_HW_CDC
  if (cdcIsConnect() && usbGetType() == USB_CON_CLI)
  {
    cli_ch = HW_UART_CH_USB;
  }
  else if (cli_ch == HW_UART_CH_USB)
  {
    cli_ch = HW_UART_CH_CLI;
  }
#endif

  // 물리 UART 로 입력이 들어오면 항상 그쪽을 우선한다.
  //
  if (uartAvailable(HW_UART_CH_CLI))
  {
    cli_ch = HW_UART_CH_CLI;
  }

  if (cliGetPort() != cli_ch)
  {
    cliOpen(cli_ch, cli_baud);
    logOpen(cli_ch, cli_baud);
  }
}

MODULE_DEF(cli){
  .name     = "cli",
  .priority = MODULE_PRI_LOW,
  .init     = cliMgrInit,
  .update   = cliMgrThread,
};

#endif
