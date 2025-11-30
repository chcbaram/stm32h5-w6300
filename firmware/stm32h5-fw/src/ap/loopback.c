#include "loopback.h"


static int8_t loopback_mode = 0;

int8_t set_loopback_mode_W6x00(uint8_t get_loopback_mode)
{
  loopback_mode = get_loopback_mode;
  return 0;
}

int8_t check_loopback_mode_W6x00()
{
  if (loopback_mode != AS_IPV4 && loopback_mode != AS_IPV6 && loopback_mode != AS_IPDUAL)
  {
    loopback_mode = AS_IPV4;
  }
  return loopback_mode;
}

// static uint16_t j=0;
static uint16_t any_port      = 50000;
static uint8_t  curr_state[8] = {
  0,
};
static uint8_t sock_state[8] = {
  0,
};
uint8_t *msg_v4   = "IPv4 mode";
uint8_t *msg_v6   = "IPv6 mode";
uint8_t *msg_dual = "Dual IP mode";

int32_t loopback_tcps(uint8_t sn, uint8_t *buf, uint16_t port)
{
  // by Lihan 20241119
  check_loopback_mode_W6x00();
  int32_t  ret;
  uint16_t sentsize = 0;
  int8_t   status, inter;
  uint8_t  tmp = 0;
  uint16_t received_size;
  uint8_t  arg_tmp8;
  uint8_t *mode_msg;

  if (loopback_mode == AS_IPV4)
  {
    mode_msg = msg_v4;
  }
  else if (loopback_mode == AS_IPV6)
  {
    mode_msg = msg_v6;
  }
  else
  {
    mode_msg = msg_dual;
  }

  getsockopt(sn, SO_STATUS, &status);
  switch (status)
  {
    case SOCK_ESTABLISHED:
      ctlsocket(sn, CS_GET_INTERRUPT, &inter);
      if (inter & Sn_IR_CON)
      {
        arg_tmp8 = Sn_IR_CON;
        ctlsocket(sn, CS_CLR_INTERRUPT, &arg_tmp8);
      }
      getsockopt(sn, SO_RECVBUF, &received_size);

      if (received_size > 0)
      {
        // logPrintf("r %d\n", received_size);
        if (received_size > DATA_BUF_SIZE) received_size = DATA_BUF_SIZE;
        ret = recv(sn, buf, received_size);


        // uint8_t sn_status;

        // getsockopt(sn, SO_EXTSTATUS, &sn_status);
        // if (sn_status & TCPSOCK_MODE)
        // {
        //   printf("Socket %d Received %d bytes\r\n", sn, received_size);
        // }
        // else
        // {
        //   printf("Socket %d Received %d bytes\r\n", sn, received_size);
        // }


        if (ret <= 0) return ret; // check SOCKERR_BUSY & SOCKERR_XXX. For showing the occurrence of SOCKERR_BUSY.
        received_size = (uint16_t)ret;
        sentsize      = 0;

        while (received_size != sentsize)
        {
          ret = send(sn, buf + sentsize, received_size - sentsize);
          // logPrintf("send %d %d\n", received_size - sentsize, ret);
          if (ret < 0)
          {
            close(sn);
            return ret;
          }
          sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
        }
// #if 1        
//         getsockopt(sn, SO_EXTSTATUS, &sn_status);

//         // 20231018 taylor
//         if (sn_status & TCPSOCK_MODE)
//         {
//           printf("Socket %d Sent back %d bytes\r\n", sn, sentsize);
//         }
//         else
//         {
//           printf("Socket %d Sent back %d bytes\r\n", sn, sentsize);
//         }
// #endif
      }
      break;

    case SOCK_CLOSE_WAIT:
      // #ifdef _LOOPBACK_DEBUG_
      printf("%d:CloseWait\r\n", sn);
      // #endif
      getsockopt(sn, SO_RECVBUF, &received_size);
      if (received_size > 0)      // Don't need to check SOCKERR_BUSY because it doesn't not occur.
      {
        if (received_size > DATA_BUF_SIZE) received_size = DATA_BUF_SIZE;
        ret = recv(sn, buf, received_size);

        if (ret <= 0) return ret; // check SOCKERR_BUSY & SOCKERR_XXX. For showing the occurrence of SOCKERR_BUSY.
        received_size = (uint16_t)ret;
        sentsize      = 0;

#if 1
        // 20231018 taylor
#ifdef _LOOPBACK_DEBUG_
        getsockopt(sn, SO_DESTIP, destip);
        getsockopt(sn, SO_DESTPORT, &destport);
        getsockopt(sn, SO_EXTSTATUS, &sn_status);
        if (sn_status & TCPSOCK_MODE)
        {
          printf("Socket %d Received %d bytes from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d : \r\n",
                 sn, received_size,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }
        else
        {
          printf("Socket %d Received %d bytes from %d.%d.%d.%d port %d : \r\n", sn, received_size, destip[0], destip[1], destip[2], destip[3], destport);
        }

        int i;
        for (i = 0; i < received_size; i++)
        {
          printf("%c", buf[i]);
        }
        printf("\r\n");
#endif
#endif

        while (received_size != sentsize)
        {
          ret = send(sn, buf + sentsize, received_size - sentsize);
          if (ret < 0)
          {
            close(sn);
            return ret;
          }
          sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
        }
#if 1
        // 20231018 taylor
#ifdef _LOOPBACK_DEBUG_
        if (sn_status & TCPSOCK_MODE)
        {
          printf("Socket %d Sent back %d bytes from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d : \r\n",
                 sn, sentsize,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }
        else
        {
          printf("Socket %d Sent back %d bytes from %d.%d.%d.%d port %d : \r\n", sn, sentsize, destip[0], destip[1], destip[2], destip[3], destport);
        }

        int j;
        for (j = 0; j < sentsize; j++)
        {
          printf("%c", buf[j]);
        }
        printf("\r\n");
#endif
#endif
      }

      if ((ret = disconnect(sn)) != SOCK_OK) return ret;
#ifdef _LOOPBACK_DEBUG_
      printf("%d:Socket Closed\r\n", sn);
#endif
      break;
    case SOCK_INIT:
      if ((ret = listen(sn)) != SOCK_OK) return ret;
#if 1
      // 20231018 taylor
#if 0
#endif
#else
#ifdef _LOOPBACK_DEBUG_
      printf("%d:Listen, TCP server loopback, port [%d] as %s\r\n", sn, port, mode_msg);
#endif
#endif
      printf("%d:Listen, TCP server loopback, port [%d] as %s\r\n", sn, port, mode_msg);
      break;
    case SOCK_CLOSED:
#ifdef _LOOPBACK_DEBUG_
      printf("%d:TCP server loopback start\r\n", sn);
#endif
      switch (loopback_mode)
      {
        case AS_IPV4:
          tmp = socket(sn, Sn_MR_TCP4, port, SOCK_IO_NONBLOCK);
          break;
        case AS_IPV6:
          tmp = socket(sn, Sn_MR_TCP6, port, SOCK_IO_NONBLOCK);
          break;
        case AS_IPDUAL:
          tmp = socket(sn, Sn_MR_TCPD, port, SOCK_IO_NONBLOCK);
          break;
        default:
          break;
      }
      if (tmp != sn) /* reinitialize the socket */
      {
#ifdef _LOOPBACK_DEBUG_
        printf("%d : Fail to create socket.\r\n", sn);
#endif
        return SOCKERR_SOCKNUM;
      }
#ifdef _LOOPBACK_DEBUG_
      printf("%d:Socket opened[%d]\r\n", sn, getSn_SR(sn));
      sock_state[sn] = 1;
#endif
      break;
    default:
      break;
  }
  return 1;
}

int32_t loopback_tcpc(uint8_t sn, uint8_t *buf, uint8_t *destip, uint16_t destport)
{
  check_loopback_mode_W6x00();
  int32_t       ret; // return value for SOCK_ERRORs
  uint16_t      sentsize = 0;
  uint8_t       status, inter, addr_len;
  uint16_t      received_size;
  uint8_t       tmp = 0;
  uint8_t       arg_tmp8;
  wiz_IPAddress destinfo;
#if 1
  // 20231018 taylor
  uint8_t sn_status;
#endif


  // Socket Status Transitions
  // Check the W6100 Socket n status register (Sn_SR, The 'Sn_SR' controlled by Sn_CR command or Packet send/recv status)
  getsockopt(sn, SO_STATUS, &status);
  switch (status)
  {
    case SOCK_ESTABLISHED:
      ctlsocket(sn, CS_GET_INTERRUPT, &inter);
      if (inter & Sn_IR_CON) // Socket n interrupt register mask; TCP CON interrupt = connection with peer is successful
      {
#ifdef _LOOPBACK_DEBUG_
#if 1
        // 20231018 taylor
        getsockopt(sn, SO_EXTSTATUS, &sn_status);
        if (sn_status & TCPSOCK_MODE)
        {
          printf("%d:Connected to - %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x : %d\r\n", sn,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }
        else
        {
          printf("%d:Connected to - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
        }
#else
        printf("%d:Connected to - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
#endif
        arg_tmp8 = Sn_IR_CON;
        ctlsocket(sn, CS_CLR_INTERRUPT, &arg_tmp8); // this interrupt should be write the bit cleared to '1'
      }

      //////////////////////////////////////////////////////////////////////////////////////////////
      // Data Transaction Parts; Handle the [data receive and send] process
      //////////////////////////////////////////////////////////////////////////////////////////////
      getsockopt(sn, SO_RECVBUF, &received_size);

      if (received_size > 0)                                              // Sn_RX_RSR: Socket n Received Size Register, Receiving data length
      {
        if (received_size > DATA_BUF_SIZE) received_size = DATA_BUF_SIZE; // DATA_BUF_SIZE means user defined buffer size (array)
        ret = recv(sn, buf, received_size);                               // Data Receive process (H/W Rx socket buffer -> User's buffer)

        if (ret <= 0) return ret;                                         // If the received data length <= 0, receive failed and process end
        received_size = (uint16_t)ret;
        sentsize      = 0;

#if 1
// 20231018 taylor
#ifdef _LOOPBACK_DEBUG_
        getsockopt(sn, SO_EXTSTATUS, &sn_status);
        if (sn_status & TCPSOCK_MODE)
        {
          printf("Socket %d Received %d bytes from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d : \r\n",
                 sn, received_size,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }
        else
        {
          printf("Socket %d Received %d bytes from %d.%d.%d.%d port %d : \r\n", sn, received_size, destip[0], destip[1], destip[2], destip[3], destport);
        }

        int i;
        for (i = 0; i < received_size; i++)
        {
          printf("%c", buf[i]);
        }
        printf("\r\n");
#endif
#endif

        // Data sentsize control
        while (received_size != sentsize)
        {
          ret = send(sn, buf + sentsize, received_size - sentsize); // Data send process (User's buffer -> Destination through H/W Tx socket buffer)
          if (ret < 0)                                              // Send Error occurred (sent data length < 0)
          {
            close(sn);                                              // socket close
            return ret;
          }
          sentsize += ret;                                          // Don't care SOCKERR_BUSY, because it is zero.
        }
#if 1
// 20231018 taylor
#ifdef _LOOPBACK_DEBUG_
        if (sn_status & TCPSOCK_MODE)
        {
          printf("Socket %d Sent back %d bytes from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d : \r\n",
                 sn, sentsize,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }
        else
        {
          printf("Socket %d Sent back %d bytes from %d.%d.%d.%d port %d : \r\n", sn, sentsize, destip[0], destip[1], destip[2], destip[3], destport);
        }

        int j;
        for (j = 0; j < sentsize; j++)
        {
          printf("%c", buf[j]);
        }
        printf("\r\n");
#endif
#endif
      }
      //////////////////////////////////////////////////////////////////////////////////////////////
      break;

    case SOCK_CLOSE_WAIT:
#ifdef _LOOPBACK_DEBUG_
      printf("%d:CloseWait\r\n", sn);
#endif
      getsockopt(sn, SO_RECVBUF, &received_size);

      if ((received_size = getSn_RX_RSR(sn)) > 0)                         // Sn_RX_RSR: Socket n Received Size Register, Receiving data length
      {
        if (received_size > DATA_BUF_SIZE) received_size = DATA_BUF_SIZE; // DATA_BUF_SIZE means user defined buffer size (array)
        ret = recv(sn, buf, received_size);                               // Data Receive process (H/W Rx socket buffer -> User's buffer)

        if (ret <= 0) return ret;                                         // If the received data length <= 0, receive failed and process end
        received_size = (uint16_t)ret;
        sentsize      = 0;
#if 1
// 20231018 taylor
#ifdef _LOOPBACK_DEBUG_
        getsockopt(sn, SO_EXTSTATUS, &sn_status);
        if (sn_status & TCPSOCK_MODE)
        {
          printf("Socket %d Received %d bytes from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d : \r\n",
                 sn, received_size,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }
        else
        {
          printf("Socket %d Received %d bytes from %d.%d.%d.%d port %d : \r\n", sn, received_size, destip[0], destip[1], destip[2], destip[3], destport);
        }

        int i;
        for (i = 0; i < received_size; i++)
        {
          printf("%c", buf[i]);
        }
        printf("\r\n");
#endif
#endif

        // Data sentsize control
        while (received_size != sentsize)
        {
          ret = send(sn, buf + sentsize, received_size - sentsize); // Data send process (User's buffer -> Destination through H/W Tx socket buffer)
          if (ret < 0)                                              // Send Error occurred (sent data length < 0)
          {
            close(sn);                                              // socket close
            return ret;
          }
          sentsize += ret;                                          // Don't care SOCKERR_BUSY, because it is zero.
        }
#if 1
// 20231018 taylor
#ifdef _LOOPBACK_DEBUG_
        if (sn_status & TCPSOCK_MODE)
        {
          printf("Socket %d Sent back %d bytes from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d : \r\n",
                 sn, sentsize,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }
        else
        {
          printf("Socket %d Sent back %d bytes from %d.%d.%d.%d port %d : \r\n", sn, sentsize, destip[0], destip[1], destip[2], destip[3], destport);
        }

        int j;
        for (j = 0; j < sentsize; j++)
        {
          printf("%c", buf[j]);
        }
        printf("\r\n");
#endif
#endif
      }
      if ((ret = disconnect(sn)) != SOCK_OK) return ret;
#ifdef _LOOPBACK_DEBUG_
      printf("%d:Socket Closed\r\n", sn);
#endif
      break;

    case SOCK_INIT:
#ifdef _LOOPBACK_DEBUG_
      if (loopback_mode == AS_IPV4)
        printf("%d:Try to connect to the %d.%d.%d.%d, %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
      else if (loopback_mode == AS_IPV6)
      {
        printf("%d:Try to connect to the %04X:%04X", sn, ((uint16_t)destip[0] << 8) | ((uint16_t)destip[1]),
               ((uint16_t)destip[2] << 8) | ((uint16_t)destip[3]));
        printf(":%04X:%04X", ((uint16_t)destip[4] << 8) | ((uint16_t)destip[5]),
               ((uint16_t)destip[6] << 8) | ((uint16_t)destip[7]));
        printf(":%04X:%04X", ((uint16_t)destip[8] << 8) | ((uint16_t)destip[9]),
               ((uint16_t)destip[10] << 8) | ((uint16_t)destip[11]));
        printf(":%04X:%04X,", ((uint16_t)destip[12] << 8) | ((uint16_t)destip[13]),
               ((uint16_t)destip[14] << 8) | ((uint16_t)destip[15]));
        printf("%d\r\n", destport);
      }
#endif

      if (loopback_mode == AS_IPV4)
        ret = connect(sn, destip, destport, 4);  /* Try to connect to TCP server(Socket, DestIP, DestPort) */
      else if (loopback_mode == AS_IPV6)
        ret = connect(sn, destip, destport, 16); /* Try to connect to TCP server(Socket, DestIP, DestPort) */

      printf("SOCK Status: %d\r\n", ret);

      if (ret != SOCK_OK) return ret;            //	Try to TCP connect to the TCP server (destination)
      break;

    case SOCK_CLOSED:
      switch (loopback_mode)
      {
        case AS_IPV4:
          tmp = socket(sn, Sn_MR_TCP4, any_port++, SOCK_IO_NONBLOCK);
          break;
        case AS_IPV6:
          tmp = socket(sn, Sn_MR_TCP6, any_port++, SOCK_IO_NONBLOCK);
          break;
        case AS_IPDUAL:
          tmp = socket(sn, Sn_MR_TCPD, any_port++, SOCK_IO_NONBLOCK);
          break;
        default:
          break;
      }

      if (tmp != sn)
      { /* reinitialize the socket */
#ifdef _LOOPBACK_DEBUG_
        printf("%d : Fail to create socket.\r\n", sn);
#endif
        return SOCKERR_SOCKNUM;
      }
      printf("%d:Socket opened[%d]\r\n", sn, getSn_SR(sn));
      sock_state[sn] = 1;

      break;
    default:
      break;
  }
  return 1;
}

int32_t loopback_udps(uint8_t sn, uint8_t *buf, uint16_t port)
{
  check_loopback_mode_W6x00();
  uint8_t        status;
  static uint8_t destip[16] = {
    0,
  };
  static uint16_t destport;
  uint8_t         pack_info;
  uint8_t         addr_len;
  uint16_t        ret;
  uint16_t        received_size;
  uint16_t        size, sentsize;
  uint8_t        *mode_msg;

  if (loopback_mode == AS_IPV4)
  {
    mode_msg = msg_v4;
  }
  else if (loopback_mode == AS_IPV6)
  {
    mode_msg = msg_v6;
  }
  else
  {
    mode_msg = msg_dual;
  }

  getsockopt(sn, SO_STATUS, &status);
  switch (status)
  {
    case SOCK_UDP:
      getsockopt(sn, SO_RECVBUF, &received_size);
      if (received_size > DATA_BUF_SIZE) received_size = DATA_BUF_SIZE;
      if (received_size > 0)
      {
        ret = recvfrom(sn, buf, received_size, (uint8_t *)&destip, (uint16_t *)&destport, &addr_len);

        if (ret <= 0)
          return ret;
        received_size = (uint16_t)ret;
#if 1
// 20231018 taylor
#ifdef _LOOPBACK_DEBUG_
        if (addr_len == 4)
        {
          printf("Socket %d Received %d bytes from %d.%d.%d.%d port %d : \r\n", sn, received_size, destip[0], destip[1], destip[2], destip[3], destport);
        }
        else
        {
          printf("Socket %d Received %d bytes from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d : \r\n",
                 sn, received_size,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }

        int i;
        for (i = 0; i < received_size; i++)
        {
          printf("%c", buf[i]);
        }
        printf("\r\n");
#endif
#endif
        // sentsize = 0;
        // while (sentsize != received_size)
        // {
        //   ret = sendto(sn, buf + sentsize, received_size - sentsize, destip, destport, addr_len);

        //   if (ret < 0) return ret;

        //   sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
        // }
#if 1
// 20231018 taylor
#ifdef _LOOPBACK_DEBUG_
        int j;
        if (addr_len == 4)
        {
          printf("Socket %d Sent back %d bytes from %d.%d.%d.%d port %d : \r\n", sn, sentsize, destip[0], destip[1], destip[2], destip[3], destport);
        }
        else
        {
          printf("Socket %d Sent back %d bytes from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d : \r\n",
                 sn, sentsize,
                 destip[0], destip[1], destip[2], destip[3],
                 destip[4], destip[5], destip[6], destip[7],
                 destip[8], destip[9], destip[10], destip[11],
                 destip[12], destip[13], destip[14], destip[15],
                 destport);
        }
        for (j = 0; j < sentsize; j++)
        {
          printf("%c", buf[j]);
        }
        printf("\r\n");
#endif
#endif
      }
      break;
    case SOCK_CLOSED:

      switch (loopback_mode)
      {
        case AS_IPV4:
          socket(sn, Sn_MR_UDP4, port, SOCK_IO_NONBLOCK);
          break;
        case AS_IPV6:
          socket(sn, Sn_MR_UDP6, port, SOCK_IO_NONBLOCK);
          break;
        case AS_IPDUAL:
          socket(sn, Sn_MR_UDPD, port, SOCK_IO_NONBLOCK);
          break;
      }
      printf("%d:Opened, UDP loopback, port [%d] as %s\r\n", sn, port, mode_msg);
  }

  return 0;
}

int32_t loopback_udpc(uint8_t sn, uint8_t *buf, uint8_t *destip, uint16_t destport)
{
  check_loopback_mode_W6x00();
  int32_t         ret;
  uint16_t        size = 0, sentsize = 0;
  static uint16_t any_port = 50000;
  uint8_t         addr_len;


  uint8_t *mode_msg;
  if (loopback_mode == AS_IPV4)
  {
    mode_msg = msg_v4;
  }
  else if (loopback_mode == AS_IPV6)
  {
    mode_msg = msg_v6;
  }
  else
  {
    mode_msg = msg_dual;
  }

  // uint8_t* strtest = "\r\nhello world";
  // uint8_t flag = 0;
  switch (getSn_SR(sn))
  {
    case SOCK_UDP:
      // sendto(sn, strtest, strlen(strtest), destip, destport);
      if ((size = getSn_RX_RSR(sn)) > 0)
      {
        if (size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
        ret      = recvfrom(sn, buf, size, destip, (uint16_t *)&destport, &addr_len);
        buf[ret] = 0x00;
        printf("recv form[%d.%d.%d.%d][%d]: %s\n", destip[0], destip[1], destip[2], destip[3], destport, buf);
        if (ret <= 0)
        {
#ifdef _LOOPBACK_DEBUG_
          printf("%d: recvfrom error. %ld\r\n", sn, ret);
#endif
          return ret;
        }
        size     = (uint16_t)ret;
        sentsize = 0;
        while (sentsize != size)
        {
          ret = sendto(sn, buf + sentsize, size - sentsize, destip, destport, addr_len);
          if (ret < 0)
          {
#ifdef _LOOPBACK_DEBUG_
            printf("%d: sendto error. %ld\r\n", sn, ret);
#endif
            return ret;
          }
          sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
        }
      }
      break;
    case SOCK_CLOSED:
#ifdef _LOOPBACK_DEBUG_
      // printf("%d:UDP loopback start\r\n",sn);
#endif

      switch (loopback_mode)
      {
        case AS_IPV4:
          socket(sn, Sn_MR_UDP4, destport, SOCK_IO_NONBLOCK);
          break;
        case AS_IPV6:
          socket(sn, Sn_MR_UDP6, destport, SOCK_IO_NONBLOCK);
          break;
        case AS_IPDUAL:
          socket(sn, Sn_MR_UDPD, destport, SOCK_IO_NONBLOCK);
          break;
      }
      printf("%d:Opened, UDP loopback, port [%d] as %s\r\n", sn, destport, mode_msg);
#ifdef _LOOPBACK_DEBUG_
      printf("%d:Opened, UDP loopback, port [%d]\r\n", sn, any_port);
#endif
      break;
    default:
      break;
  }
  return 1;
}
