#include "cmd_task.h"

#ifdef _USE_HW_CMD
#include "qbuffer.h"


//-- cmd 패킷 위에 얹는 가상 CLI 채널.
//
//   앱의 텔넷 CLI(cli_net.c)와 같은 방식이다. uartSetDriver() 로 가상 UART 채널을
//   등록하면 cli.c 는 아무것도 모른 채 그대로 동작하고, cli_mgr 의 포트 자동 전환에
//   한 줄만 추가하면 된다.
//
//   전송이 HID 든 CDC 든 (향후) W6300 이든 상관없다. cmd.c 가 전송계층과 무관하고
//   이 채널은 그 위에 있기 때문이다.
//
#define CLI_RX_BUF_SIZE   512
#define CLI_TX_BUF_SIZE   1024

static uint8_t   rx_buf[CLI_RX_BUF_SIZE];
static qbuffer_t rx_q;

static uint8_t   tx_buf[CLI_TX_BUF_SIZE];
static uint32_t  tx_len = 0;

static cmd_t    *p_cli_cmd = NULL;      // 마지막으로 CLI 명령을 보낸 채널
static bool      is_init   = false;
static uart_driver_t cli_drv;


static bool     cliCmdOpen(uint32_t baud);
static bool     cliCmdClose(void);
static uint32_t cliCmdAvailable(void);
static bool     cliCmdFlush(void);
static uint8_t  cliCmdRead(void);
static uint32_t cliCmdWrite(uint8_t *p_data, uint32_t length);



bool cliCmdInit(void)
{
  qbufferCreate(&rx_q, rx_buf, CLI_RX_BUF_SIZE);
  tx_len = 0;

  cli_drv.open      = cliCmdOpen;
  cli_drv.close     = cliCmdClose;
  cli_drv.available = cliCmdAvailable;
  cli_drv.flush     = cliCmdFlush;
  cli_drv.read      = cliCmdRead;
  cli_drv.write     = cliCmdWrite;

  uartSetDriver(HW_UART_CH_CMD, &cli_drv);

  is_init = true;
  return true;
}

bool cliCmdIsConnected(void)
{
  return (p_cli_cmd != NULL);
}

//-- 호스트가 보낸 CLI 입력을 밀어넣는다.
//
//   응답을 어느 채널로 돌려줄지 기억해 둔다. HID 로 친 명령의 출력이 CDC 로
//   나가면 안 되기 때문이다.
//
bool cliCmdPutLine(cmd_t *p_cmd, uint8_t *p_data, uint32_t length)
{
  if (is_init != true)
    cliCmdInit();

  p_cli_cmd = p_cmd;
  tx_len    = 0;

  qbufferWrite(&rx_q, p_data, length);

  // cli.c 는 CR(0x0D)을 엔터로 본다. 호스트가 빠뜨렸으면 붙여준다.
  if (length == 0 || p_data[length - 1] != '\r')
  {
    uint8_t cr = '\r';
    qbufferWrite(&rx_q, &cr, 1);
  }
  return true;
}

//-- 명령 처리가 끝난 뒤 모인 출력을 돌려준다.
//
uint32_t cliCmdGetOut(uint8_t **pp_data)
{
  *pp_data = tx_buf;
  return tx_len;
}

void cliCmdClearOut(void)
{
  tx_len = 0;
}


bool cliCmdOpen(uint32_t baud)
{
  (void)baud;
  return true;
}

bool cliCmdClose(void)
{
  return true;
}

uint32_t cliCmdAvailable(void)
{
  return qbufferAvailable(&rx_q);
}

bool cliCmdFlush(void)
{
  qbufferFlush(&rx_q);
  return true;
}

uint8_t cliCmdRead(void)
{
  uint8_t data = 0;

  qbufferRead(&rx_q, &data, 1);
  return data;
}

//-- cli.c 의 출력은 여기로 모인다.
//   USB 콜백 밖에서 불리므로 그대로 버퍼에 쌓았다가 한 번에 응답으로 보낸다.
//
uint32_t cliCmdWrite(uint8_t *p_data, uint32_t length)
{
  uint32_t n = length;

  if (tx_len + n > CLI_TX_BUF_SIZE)
    n = CLI_TX_BUF_SIZE - tx_len;

  if (n > 0)
  {
    memcpy(&tx_buf[tx_len], p_data, n);
    tx_len += n;
  }
  return length;      // 넘쳐도 성공으로 처리한다. CLI 를 막지 않기 위해서다.
}

#endif
