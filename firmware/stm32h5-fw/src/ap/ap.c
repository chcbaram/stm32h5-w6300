#include "ap.h"
#include "loopback.h"

// #define ETHERNET_BUF_MAX_SIZE (4 * 1024)

#define ETHERNET_BUF_MAX_SIZE DATA_BUF_SIZE

#define SOCKET_TCP_SERVER     HW_WIZNET_SOCKET_TCP


/* Port */
#define PORT_TCP_SERVER       5001
#define PORT_TCP_CLIENT       5001
#define PORT_TCP_CLIENT_DEST  5002
#define PORT_UDP              5003

#define PORT_TCP_SERVER6      5004
#define PORT_TCP_CLIENT6      5005
#define PORT_TCP_CLIENT6_DEST 5006
#define PORT_UDP6             5007

#define PORT_TCP_SERVER_DUAL  5008



static uint8_t g_tcp_server_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};

static void updateLED(void);
static void updateWiznet(void);
int32_t iperf_tcps(uint8_t sn, uint8_t *buf, uint16_t port);



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

    /* TCP server loopback test */
    // if ((retval = loopback_tcps(SOCKET_TCP_SERVER, g_tcp_server_buf, PORT_TCP_SERVER)) < 0)
    // if ((retval = loopback_udps(SOCKET_TCP_SERVER, g_tcp_server_buf, PORT_TCP_SERVER)) < 0)
    if ((retval = iperf_tcps(SOCKET_TCP_SERVER, g_tcp_server_buf, PORT_TCP_SERVER)) < 0)   
    {
      logPrintf(" loopback_tcps error : %d\n", retval);

      while (1)
        ;
    }
 }
}

int32_t iperf_tcps(uint8_t sn, uint8_t *buf, uint16_t port)
{
  int32_t  ret;
  uint16_t size = 0, sentsize = 0;

  uint8_t  destip[4];
  uint16_t destport;

  switch (getSn_SR(sn))
  {
    case SOCK_ESTABLISHED:

      if (getSn_IR(sn) & Sn_IR_CON)
      {
        getSn_DIPR(sn, destip);
        destport = getSn_DPORT(sn);

        logPrintf("%d:Connected - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
        setSn_IR(sn, Sn_IR_CON);
      }      

      if ((size = getSn_RX_RSR(sn)) > 0) 
      {
        if (size > DATA_BUF_SIZE)
        {
          size = DATA_BUF_SIZE;    // DATA_BUF_SIZE means user defined buffer size (array)
        }
        ret = recv(sn, buf, size); // Data Receive process (H/W Rx socket buffer -> User's buffer)

        if (ret <= 0)
        {
          return ret;              // If the received data length <= 0, receive failed and process end
        }
        
        #if 0
        sentsize = 0;

        while (size != sentsize)
        {
          ret = send(sn, buf + sentsize, size - sentsize);
          logPrintf("%d %d %d\n", sentsize, size, ret);
          if (ret < 0)
          {
            close(sn);
            return ret;
          }
          sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
        }
        #endif
      }
      break;

    case SOCK_CLOSE_WAIT:
      printf("%d:CloseWait\r\n",sn);
      if ((ret = disconnect(sn)) != SOCK_OK) 
        return ret;
      logPrintf("%d:Socket Closed\r\n", sn);
      break;

    case SOCK_INIT:
      logPrintf("%d:Listen, TCP server loopback, port [%d]\r\n", sn, port);
      if ((ret = listen(sn)) != SOCK_OK) 
        return ret;
      break;

    case SOCK_CLOSED:
      printf("%d:TCP server loopback start\r\n",sn);
      if ((ret = socket(sn, Sn_MR_TCP, port, 0x00)) != sn) 
        return ret;
      printf("%d:Socket Opened\r\n",sn);
      break;

    default:
      break;
  }
  return 1;
}
