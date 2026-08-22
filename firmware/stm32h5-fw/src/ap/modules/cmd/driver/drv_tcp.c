#include "cmd_task.h"

#if defined(_USE_HW_CMD) && defined(_USE_HW_WIZNET)
#include "qbuffer.h"


//-- TCP 채널 드라이버 (이더넷 OTA).
//
//   cmd.c 가 전송계층과 무관하게 설계되어 있어서, 여섯 함수만 채우면 CDC/HID 와
//   **같은 커맨드 셋**이 네트워크에서 그대로 돈다. 부트로더 쪽은 손댈 것이 없다 -
//   앱이 슬롯에 받아두고 resetToUpdate() 하면 다음 부팅에 부트로더가 적용한다.
//
//   소켓에서 바이트 단위로 읽으면 매번 SPI 트랜잭션이 생긴다. cmd.c 는 한 바이트씩
//   읽어가므로, 여기서 한 번에 끌어와 링버퍼에 쌓고 그걸 내준다.
//
#define TCP_SN            HW_WIZNET_SOCKET_TCP    // 3
#define TCP_PORT          5301
#define TCP_RX_BUF_SIZE   2048

static uint8_t   rx_buf[TCP_RX_BUF_SIZE];
static qbuffer_t rx_q;
static bool      is_init      = false;
static bool      is_connected = false;

static void drvTcpInitOnce(void);


//-- 소켓 상태를 돌보고 수신분을 링버퍼에 옮긴다.
//   cmdTaskUpdate() 가 매번 부른다.
//
void drvTcpUpdate(void)
{
  uint8_t sr;

  if (is_init != true || wiznetIsInit() != true)
    return;

  sr = getSn_SR(TCP_SN);

  if (sr == SOCK_CLOSED)
  {
    is_connected = false;
    if (socket(TCP_SN, Sn_MR_TCP, TCP_PORT, SF_IO_NONBLOCK) == TCP_SN)
      listen(TCP_SN);
    return;
  }

  if (sr == SOCK_ESTABLISHED)
  {
    uint16_t len = getSn_RX_RSR(TCP_SN);

    if (is_connected != true)
    {
      is_connected = true;
      qbufferFlush(&rx_q);      // 이전 연결의 찌꺼기를 남기지 않는다
      logPrintf("[  ] cmd tcp connected\n");
    }

    while (len > 0)
    {
      uint8_t  buf[256];
      uint16_t room = (uint16_t)(TCP_RX_BUF_SIZE - qbufferAvailable(&rx_q) - 1);
      uint16_t n    = len;

      if (n > sizeof(buf)) n = sizeof(buf);
      if (n > room)        n = room;
      if (n == 0)          break;      // 링버퍼가 찼다. 다음 기회에 가져간다

      if (recv(TCP_SN, buf, n) != (int32_t)n)
        break;

      qbufferWrite(&rx_q, buf, n);
      len -= n;
    }
    return;
  }

  if (sr == SOCK_CLOSE_WAIT)
  {
    disconnect(TCP_SN);
    close(TCP_SN);
    is_connected = false;
    logPrintf("[  ] cmd tcp disconnected\n");
  }
}

bool drvTcpIsConnected(void)
{
  return is_connected;
}

static void drvTcpInitOnce(void)
{
  if (is_init)
    return;
  qbufferCreate(&rx_q, rx_buf, TCP_RX_BUF_SIZE);
  is_init = true;
}


static bool drvTcpOpen(void *args)
{
  (void)args;
  drvTcpInitOnce();
  return true;
}

static bool drvTcpClose(void *args)
{
  (void)args;
  return true;
}

static uint32_t drvTcpAvailable(void *args)
{
  (void)args;
  drvTcpInitOnce();
  return qbufferAvailable(&rx_q);
}

static bool drvTcpFlush(void *args)
{
  (void)args;
  drvTcpInitOnce();
  qbufferFlush(&rx_q);
  return true;
}

static uint8_t drvTcpRead(void *args)
{
  uint8_t data = 0;

  (void)args;
  drvTcpInitOnce();
  qbufferRead(&rx_q, &data, 1);
  return data;
}

static uint32_t drvTcpWrite(void *args, uint8_t *p_data, uint32_t length)
{
  uint32_t sent = 0;

  (void)args;

  if (is_connected != true)
    return 0;

  while (sent < length)
  {
    int32_t ret = send(TCP_SN, &p_data[sent], (uint16_t)(length - sent));

    if (ret <= 0)
      break;
    sent += (uint32_t)ret;
  }
  return sent;
}


cmd_driver_t drv_tcp_driver =
{
  .open      = drvTcpOpen,
  .close     = drvTcpClose,
  .available = drvTcpAvailable,
  .flush     = drvTcpFlush,
  .read      = drvTcpRead,
  .write     = drvTcpWrite,
};

#endif
