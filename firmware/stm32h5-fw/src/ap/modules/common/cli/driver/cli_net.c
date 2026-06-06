#include "cli_net.h"


// telnet 명령 바이트
#define TN_IAC  255
#define TN_DONT 254
#define TN_DO   253
#define TN_WONT 252
#define TN_WILL 251
#define TN_SB   250
#define TN_SE   240
#define TN_OPT_ECHO 1
#define TN_OPT_SGA  3

enum
{
  IAC_NORMAL,
  IAC_CMD,    
  IAC_OPT,    
  IAC_SB,     
  IAC_SB_IAC, 
};

static uint16_t net_port    = 23;
// WIZnet에서는 고정된 하드웨어 소켓 채널(예: 0번 소켓)을 사용합니다.
static const uint8_t cli_sn = 0; 

static uint8_t  rx_buf[256];
static uint16_t rx_len   = 0;
static uint16_t rx_idx   = 0;
static uint8_t  iac_state = IAC_NORMAL;
static bool     prev_cr  = false; 
static uint8_t  tx_prev  = 0;     

// WIZnet용 세션 연결 상태 플래그
static bool          is_connected = false;
static uart_driver_t net_drv;

static bool     _open(uint32_t baud);
static bool     _close(void);
static uint32_t _available(void);
static bool     _flush(void);
static uint8_t  _read(void);
static uint32_t _write(uint8_t *p_data, uint32_t length);
static void     clientClose(void);



bool cliNetInit(uint16_t port)
{
  net_port     = port;
  is_connected = false;

  net_drv.open      = _open;
  net_drv.close     = _close;
  net_drv.available = _available;
  net_drv.flush     = _flush;
  net_drv.read      = _read;
  net_drv.write     = _write;

  uartSetDriver(HW_UART_CH_NET, &net_drv);

  return true;
}

void cliNetPoll(void)
{
  uint8_t sr = getSn_SR(cli_sn);

  // 1. 소켓이 완전히 닫혀있으면 소켓 생성 및 Listen 상태로 진입
  if (sr == SOCK_CLOSED)
  {
    // 네트워크 링크 상태 확인 API가 있다면 여기서 체크 (예: wizphy_getphilink())
    
    // TCP 소켓 오픈 (논블로킹 모드로 동작하도록 설정)
    if (socket(cli_sn, Sn_MR_TCP, net_port, SF_IO_NONBLOCK) == cli_sn)
    {
      listen(cli_sn);
      logPrintf("[OK] cliNetListen() port %d\n", net_port);
    }
    return;
  }

  // 2. 클라이언트가 접속하여 ESTABLISHED 상태가 되었을 때 최초 1회 처리
  if (sr == SOCK_ESTABLISHED && !is_connected)
  {
    // char 모드 협상 데이터
    static const uint8_t nego[] = {
      TN_IAC, TN_WILL, TN_OPT_ECHO,
      TN_IAC, TN_WILL, TN_OPT_SGA,
    };

    is_connected = true;
    rx_len      = 0;
    rx_idx      = 0;
    iac_state   = IAC_NORMAL;
    prev_cr     = false;
    tx_prev     = 0;

    // 텔넷 옵션 명령 전송
    send(cli_sn, (uint8_t *)nego, sizeof(nego));
    logPrintf("[  ] cliNet client connected\n");
  }

  // 3. 상대방이 연결을 끊으려고 하거나(FIN) 비정상 종료된 경우 처리
  if (sr == SOCK_CLOSE_WAIT || sr != SOCK_ESTABLISHED)
  {
    if (is_connected || sr == SOCK_CLOSE_WAIT)
    {
      clientClose();
    }
  }
}

bool cliNetIsConnected(void)
{
  return is_connected;
}

// WIZnet 전용: 수신 버퍼에 데이터가 있는지 확인하는 함수
static bool sockReadable(uint8_t sn)
{
  // 정상 연결 상태이고 수신 바이트(RSR)가 0보다 크면 true
  return (getSn_SR(sn) == SOCK_ESTABLISHED && getSn_RX_RSR(sn) > 0);
}

static void clientClose(void)
{
  // WIZnet 소켓 연결 종료 및 다음 접속을 위한 listen 재시작 준비
  disconnect(cli_sn);
  close(cli_sn);
  
  is_connected = false;
  rx_len    = 0;
  rx_idx    = 0;
  iac_state = IAC_NORMAL;
  prev_cr   = false;
  tx_prev   = 0;
}

static void pushByte(uint8_t b)
{
  if (b == '\r')
  {
    if (rx_len < sizeof(rx_buf))
      rx_buf[rx_len++] = '\r';
    prev_cr = true;
    return;
  }

  if (b == '\n')
  {
    if (prev_cr) 
    {
      prev_cr = false;
      return;
    }
    if (rx_len < sizeof(rx_buf)) 
      rx_buf[rx_len++] = '\r';
    return;
  }

  if (b == 0x00) 
  {
    prev_cr = false;
    return;
  }

  if (b == 0x7F)
    b = 0x08;

  prev_cr = false;
  if (rx_len < sizeof(rx_buf))
    rx_buf[rx_len++] = b;
}


bool _open(uint32_t baud)
{
  (void)baud;
  return true;
}

bool _close(void)
{
  return true;
}

uint32_t _available(void)
{
  uint8_t raw[256];
  int32_t n;

  if (!is_connected)
    return 0;

  if (rx_idx < rx_len)
    return rx_len - rx_idx;

  if (!sockReadable(cli_sn))
    return 0;

  // WIZnet recv() 함수 사용
  n = recv(cli_sn, raw, sizeof(raw));
  if (n == SOCK_BUSY) // 논블로킹 모드에서 읽을 데이터 없음
  {
    return 0;
  }
  if (n == 0 || n < 0) // 연결 종료 또는 에러 발생
  {
    clientClose();
    return 0;
  }

  // telnet IAC 시퀀스 처리 루틴 (기존과 동일)
  rx_len = 0;
  rx_idx = 0;
  for (int i = 0; i < n; i++)
  {
    uint8_t b = raw[i];

    switch (iac_state)
    {
      case IAC_NORMAL:
        if (b == TN_IAC)
          iac_state = IAC_CMD;
        else
          pushByte(b);
        break;

      case IAC_CMD:
        if (b == TN_IAC) 
        {
          pushByte(TN_IAC);
          iac_state = IAC_NORMAL;
        }
        else if (b == TN_WILL || b == TN_WONT || b == TN_DO || b == TN_DONT)
          iac_state = IAC_OPT;
        else if (b == TN_SB)
          iac_state = IAC_SB;
        else
          iac_state = IAC_NORMAL; 
        break;

      case IAC_OPT:
        iac_state = IAC_NORMAL; 
        break;

      case IAC_SB:
        if (b == TN_IAC)
          iac_state = IAC_SB_IAC;
        break;

      case IAC_SB_IAC:
        iac_state = (b == TN_SE) ? IAC_NORMAL : IAC_SB;
        break;
    }
  }

  return rx_len;
}

bool _flush(void)
{
  rx_len = 0;
  rx_idx = 0;
  return true;
}

uint8_t _read(void)
{
  if (rx_idx < rx_len)
    return rx_buf[rx_idx++];

  return 0;
}

// 출력 버퍼를 모두 송신 (WIZnet 스타일)
static bool sendAll(const uint8_t *p, uint32_t n)
{
  uint32_t off = 0;

  while (off < n)
    {
    int32_t s = send(cli_sn, (uint8_t *)(p + off), n - off);
    if (s == SOCK_BUSY)
    {
      continue; // 논블로킹 전송 지연 시 재시도
    }
    if (s <= 0)
    {
      return false;
    }
    off += (uint32_t)s;
  }
  return true;
}

uint32_t _write(uint8_t *p_data, uint32_t length)
{
  uint8_t  out[256];
  uint32_t oi = 0;

  if (!is_connected)
    return 0;

  for (uint32_t i = 0; i < length; i++)
  {
    uint8_t c = p_data[i];

    if (c == '\n' && tx_prev != '\r')
    {
      out[oi++] = '\r';
      if (oi >= sizeof(out))
      {
        if (!sendAll(out, oi)) { clientClose(); return 0; }
        oi = 0;
      }
    }

    out[oi++] = c;
    tx_prev   = c;

    if (oi >= sizeof(out))
    {
      if (!sendAll(out, oi)) { clientClose(); return 0; }
      oi = 0;
    }
  }

  if (oi > 0 && !sendAll(out, oi)) { clientClose(); return 0; }

  return length;
}

