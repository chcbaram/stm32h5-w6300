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

//   출력 버퍼. `log` 덤프 같은 것이 1KB 를 훌쩍 넘긴다. 넉넉히 잡되,
//   그래도 넘칠 수 있으므로 꼬리에 잘림 표시를 넣을 자리를 남겨둔다.
#define CLI_TX_BUF_SIZE   4096
#define CLI_TX_MARK_SIZE  32



//   cliKeepLoop() 을 쓰는 반복 명령(usb info 등)을 끊어주는 시간.
//   이 채널에는 키를 눌러줄 사람이 없으므로 우리가 대신 눌러야 한다.
#define CLI_LOOP_BREAK_MS 200

static uint8_t   rx_buf[CLI_RX_BUF_SIZE];
static qbuffer_t rx_q;

static uint8_t   tx_buf[CLI_TX_BUF_SIZE];
static uint32_t  tx_len   = 0;      // 쌓인 길이
static uint32_t  tx_index = 0;      // 호스트가 읽어간 위치
static bool      is_over  = false;  // 버퍼를 넘겼다

static cmd_t    *p_cli_cmd  = NULL;     // 마지막으로 CLI 명령을 보낸 채널
static bool      is_init    = false;
static bool      is_running = false;    // 명령 실행 중
static uint32_t  run_time   = 0;
static uart_driver_t cli_drv;


static bool     drvCliOpen(uint32_t baud);
static bool     drvCliClose(void);
static uint32_t drvCliAvailable(void);
static bool     drvCliFlush(void);
static uint8_t  drvCliRead(void);
static uint32_t drvCliWrite(uint8_t *p_data, uint32_t length);



bool drvCliInit(void)
{
  qbufferCreate(&rx_q, rx_buf, CLI_RX_BUF_SIZE);
  tx_len = 0;

  cli_drv.open      = drvCliOpen;
  cli_drv.close     = drvCliClose;
  cli_drv.available = drvCliAvailable;
  cli_drv.flush     = drvCliFlush;
  cli_drv.read      = drvCliRead;
  cli_drv.write     = drvCliWrite;

  uartSetDriver(HW_UART_CH_CMD, &cli_drv);

  is_init = true;
  return true;
}

bool drvCliIsConnected(void)
{
  return (p_cli_cmd != NULL);
}

//-- 호스트가 보낸 CLI 입력을 밀어넣는다.
//
//   응답을 어느 채널로 돌려줄지 기억해 둔다. HID 로 친 명령의 출력이 CDC 로
//   나가면 안 되기 때문이다.
//
bool drvCliPutLine(cmd_t *p_cmd, uint8_t *p_data, uint32_t length)
{
  if (is_init != true)
    drvCliInit();

  p_cli_cmd  = p_cmd;
  tx_len     = 0;
  tx_index   = 0;
  is_over    = false;
  is_running = true;
  run_time   = millis();

  qbufferWrite(&rx_q, p_data, length);

  // cli.c 는 CR(0x0D)을 엔터로 본다. 호스트가 빠뜨렸으면 붙여준다.
  if (length == 0 || p_data[length - 1] != '\r')
  {
    uint8_t cr = '\r';
    qbufferWrite(&rx_q, &cr, 1);
  }
  return true;
}

//-- cliKeepLoop() 을 쓰는 반복 명령을 끊는다.
//
//   `usb info` 처럼 while(cliKeepLoop()) 로 도는 명령은 CLI 포트에 입력이 들어와야
//   빠져나온다. 이 가상 채널에는 키를 눌러줄 사람이 없어서 영원히 돈다. 그러면
//   cliMain() 이 반환하지 않고, cmd_boot.c 의 시간 예산도 소용이 없다. 게다가
//   그 안의 delay() 가 cliLoopIdle() -> moduleUpdate() 를 부르므로 다음 요청이
//   재진입 처리되어 출력이 뒤섞인다.
//
//   그래서 CLI_LOOP_BREAK_MS 가 지나면 여기서 개행 하나를 넣어 "키가 눌린" 것으로
//   만든다. 반복 명령은 화면 한 장을 뱉고 정상적으로 빠져나온다.
//
//   이 함수는 cliLoopIdle() -> moduleUpdate() 경로에서도 불려야 한다.
//   그래야 cliMain() 안에 갇혀 있는 동안에도 끊어줄 수 있다.
//
void drvCliUpdate(void)
{
  if (is_init != true || is_running != true)
    return;

  if (millis() - run_time < CLI_LOOP_BREAK_MS)
    return;

  if (qbufferAvailable(&rx_q) == 0)
  {
    uint8_t cr = '\r';
    qbufferWrite(&rx_q, &cr, 1);
  }
  is_running = false;
}

//-- 모인 출력을 조각내어 돌려준다.
//
//   한 번에 다 보낼 수 없다. cmd 패킷의 데이터 길이 상한이 있기 때문이다.
//   부를 때마다 다음 조각을 주고, 남은 것이 있으면 drvCliHasMore() 가 참이다.
//   호스트는 거짓이 될 때까지 BOOT_CMD_CLI_MORE 로 계속 가져간다.
//
uint32_t drvCliGetOut(uint8_t **pp_data, uint32_t max_len)
{
  uint32_t n = tx_len - tx_index;

  if (n > max_len)
    n = max_len;

  *pp_data  = &tx_buf[tx_index];
  tx_index += n;
  return n;
}

bool drvCliHasMore(void)
{
  return (tx_index < tx_len);
}

//-- 출력이 다 모인 시점에 잘림 여부를 표시한다.
//
//   조용히 버리면 호스트는 그게 전부인 줄 안다. 눈에 보이게 남긴다.
//
void drvCliEndOut(void)
{
  const char *p_msg = "\n... (출력이 잘렸다)\n";
  uint32_t    n;

  if (is_over != true)
    return;

  n = strlen(p_msg);
  if (tx_len + n <= CLI_TX_BUF_SIZE)
  {
    memcpy(&tx_buf[tx_len], p_msg, n);
    tx_len += n;
  }
  is_over = false;
}

void drvCliClearOut(void)
{
  tx_len     = 0;
  tx_index   = 0;
  is_over    = false;
  is_running = false;
}


bool drvCliOpen(uint32_t baud)
{
  (void)baud;
  return true;
}

bool drvCliClose(void)
{
  return true;
}

uint32_t drvCliAvailable(void)
{
  return qbufferAvailable(&rx_q);
}

bool drvCliFlush(void)
{
  qbufferFlush(&rx_q);
  return true;
}

uint8_t drvCliRead(void)
{
  uint8_t data = 0;

  qbufferRead(&rx_q, &data, 1);
  return data;
}

//-- cli.c 의 출력은 여기로 모인다.
//   USB 콜백 밖에서 불리므로 그대로 버퍼에 쌓았다가 한 번에 응답으로 보낸다.
//
uint32_t drvCliWrite(uint8_t *p_data, uint32_t length)
{
  uint32_t room = (CLI_TX_BUF_SIZE - CLI_TX_MARK_SIZE) - tx_len;
  uint32_t n    = length;

  if (n > room)
  {
    n       = room;
    is_over = true;     // 잘렸다는 사실은 drvCliEndOut() 이 꼬리에 남긴다
  }

  if (n > 0)
  {
    memcpy(&tx_buf[tx_len], p_data, n);
    tx_len += n;
  }
  return length;      // 넘쳐도 성공으로 처리한다. CLI 를 막지 않기 위해서다.
}

#endif
