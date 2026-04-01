#include "iperf.h"






int32_t iperf_tcps(uint8_t sn, uint8_t *buf, uint16_t port)
{
  int32_t  ret;
  uint16_t size     = 0;
  // uint16_t sentsize = 0;

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
        if (size > IPERF_BUF_MAX_SIZE)
        {
          size = IPERF_BUF_MAX_SIZE; // DATA_BUF_SIZE means user defined buffer size (array)
        }
        ret = recv(sn, buf, size);   // Data Receive process (H/W Rx socket buffer -> User's buffer)

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
