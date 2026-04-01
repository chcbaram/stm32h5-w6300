#include "ap.h"
#include "iperf.h"





static void updateLED(void);
static void updateWiznet(void);




void apInit(void)
{  
  logBoot(false);
  cliOpen(HW_UART_CH_CLI, 115200);  
  cliBegin();
}

void apMain(void)
{
  while(1)
  {
    cliMain();

    updateLED();    
    updateWiznet();    



  }
}

void updateLED(void)
{
  static uint32_t pre_time = 0;
  
  
  if (millis() - pre_time >= 500)
  {
    pre_time = millis();
    ledToggle(_DEF_LED1);
  }
}

void updateWiznet(void)
{
  static uint8_t iperf_buf[IPERF_BUF_MAX_SIZE] = { 0,};

  eventUpdate();
  wiznetUpdate();  


 if (wiznetIsGetIP())
 {
    int retval = 0;

    if ((retval = iperf_tcps(HW_WIZNET_SOCKET_TCP, iperf_buf, 5001)) < 0)
    {
      logPrintf(" loopback_tcps error : %d\n", retval);

      while (1)
        ;
    }
 }
}

