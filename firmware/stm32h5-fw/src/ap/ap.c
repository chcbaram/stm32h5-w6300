#include "ap.h"



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
  eventUpdate();
  wiznetUpdate();  
}
