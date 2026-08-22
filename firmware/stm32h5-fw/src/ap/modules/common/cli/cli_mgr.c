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

  if (is_enable)
  {
    cliMain();
  }

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

#ifdef _USE_HW_CDC
  if (cdcIsConnect())
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
