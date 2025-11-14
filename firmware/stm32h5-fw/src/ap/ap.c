#include "ap.h"


/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 4)
#define DATA_BUF_SIZE			ETHERNET_BUF_MAX_SIZE
/* Socket */
#define SOCKET_LOOPBACK 3

/* Port */
#define PORT_LOOPBACK 5001

#define _USE_LOOPBACK      1
#define _LOOPBACK_DEBUG_


uint8_t loopback_buf[DATA_BUF_SIZE];




static void updateLED(void);
static void updateWiznet(void);
static int32_t iperf_tcps(uint8_t sn, uint8_t* buf, uint16_t port);


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


 if (wiznetIsGetIP())
 {
    int retval = 0;
    if ((retval = iperf_tcps(SOCKET_LOOPBACK, loopback_buf, PORT_LOOPBACK)) < 0)
    {
      logPrintf(" Loopback error : %d\n", retval);

      while (1)
        ;
    }
 }
}







int32_t iperf_tcps(uint8_t sn, uint8_t* buf, uint16_t port)
{
   int32_t ret;
   uint16_t size = 0, sentsize=0;

#ifdef _LOOPBACK_DEBUG_
   uint8_t destip[4];
   uint16_t destport;
#endif

   switch(getSn_SR(sn))
   {
      case SOCK_ESTABLISHED :
         if(getSn_IR(sn) & Sn_IR_CON)
         {
#ifdef _LOOPBACK_DEBUG_
			getSn_DIPR(sn, destip);
			destport = getSn_DPORT(sn);

			logPrintf("%d:Connected - %d.%d.%d.%d : %d\r\n",sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
			setSn_IR(sn,Sn_IR_CON);
         }
		 if((size = getSn_RX_RSR(sn)) > 0) // Don't need to check SOCKERR_BUSY because it doesn't not occur.
         {
			if(size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
			ret = recv(sn, buf, size);

			if(ret <= 0) return ret;      // check SOCKERR_BUSY & SOCKERR_XXX. For showing the occurrence of SOCKERR_BUSY.
			size = (uint16_t) ret;

			

#if _USE_LOOPBACK
            sentsize = 0;

			while(size != sentsize)
			{
				ret = send(sn, buf+sentsize, size-sentsize);
				if(ret < 0)
				{
					close(sn);
					return ret;
				}
				sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
			}
      
#endif            
         }
         break;
      case SOCK_CLOSE_WAIT :
#ifdef _LOOPBACK_DEBUG_
         //printf("%d:CloseWait\r\n",sn);
#endif
         if((ret = disconnect(sn)) != SOCK_OK) return ret;
#ifdef _LOOPBACK_DEBUG_
         logPrintf("%d:Socket Closed\r\n", sn);
#endif
         break;
      case SOCK_INIT :
#ifdef _LOOPBACK_DEBUG_
    	 logPrintf("%d:Listen, TCP server loopback, port [%d]\r\n", sn, port);
#endif
         if( (ret = listen(sn)) != SOCK_OK) return ret;
         break;
      case SOCK_CLOSED:
#ifdef _LOOPBACK_DEBUG_
         //printf("%d:TCP server loopback start\r\n",sn);
#endif
         if((ret = socket(sn, Sn_MR_TCP, port, 0x00)) != sn) return ret;
#ifdef _LOOPBACK_DEBUG_
         //printf("%d:Socket opened\r\n",sn);
#endif
         break;
      default:
         break;
   }
   return 1;
}
