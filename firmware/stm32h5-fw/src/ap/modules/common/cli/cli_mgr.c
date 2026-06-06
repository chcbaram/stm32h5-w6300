#include "cli_mgr.h"
#include "driver/cli_net.h"


#ifdef _USE_HW_CLI

void cliThread(void const *arg);


static uint8_t  cli_ch    = HW_UART_CH_CLI;
static uint32_t cli_baud  = 115200;
static bool     is_enable = true;



bool cliMgrInit(void)
{
  cliNetInit(23);

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
  if (is_enable)
  {
    cliMain();
  }

  cliNetPoll();
  if (cliNetIsConnected())
  {
    cli_ch = HW_UART_CH_NET;
  }
  else if (cli_ch == HW_UART_CH_NET)
  {
    cli_ch = HW_UART_CH_CLI;
  }

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