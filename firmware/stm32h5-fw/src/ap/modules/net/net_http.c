#include "net_http.h"

#ifdef _USE_HW_WIZNET
#include "cmd.h"


//-- 보드 자체 웹서버.
//
//   같은 출처(http)로 오므로 mixed content 제약이 사라진다. GitHub Pages 에서
//   http://보드IP 로 fetch 를 걸 수 없던 문제가 여기서는 없다.
//
//   커맨드 채널은 **WebSocket 이 아니라 POST** 다. 로드맵에는 WebSocket 이라고
//   적어뒀지만 바꿨다.
//
//     - cmd 는 원래 요청/응답이다. POST 한 번에 그대로 맞아떨어진다
//     - WebSocket 은 업그레이드 핸드셰이크(SHA1+base64)와 프레이밍·마스킹 해제가
//       필요하다. MCU 에 200줄 남짓이 더 붙는데 얻는 것은 서버 푸시뿐이고,
//       우리는 그게 필요 없다
//
//   푸시가 필요해지면(로그 스트리밍 같은) 그때 WebSocket 을 더하면 된다.
//
//     GET  /              gzip 된 페이지
//     POST /cmd           본문 = cmd 패킷 원본, 응답 = 응답 패킷 원본
//
//-- 소켓을 여러 개 듣는다.
//
//   브라우저는 페이지 하나를 받으려고 index.html + 모듈 5개를 요청하고, 그중
//   여럿을 **동시에** 연다. 소켓이 하나면 나머지는 연결조차 안 되고 페이지가
//   반쯤 뜬다. 실제로 curl 을 연달아 던졌을 때 두 번째가 연결 실패했다.
//
//   W6300 은 소켓이 8개다. 0=telnet 1=DHCP 2=SNTP 3=cmd TCP 4=discovery 를 빼면
//   5,6,7 이 남는다. 셋을 다 듣는다.
//
#define HTTP_SN_FIRST     5
#define HTTP_SN_CNT       3
#define HTTP_REQ_MAX      1600      // cmd 최대 1024 + 헤더
#define HTTP_HDR_MAX      256

//   keep-alive 로 한 연결에서 여러 요청을 처리한다.
//
//   소켓을 셋 밖에 못 쓰는데 브라우저는 파일 여섯 개를 최대 6개 연결로 동시에
//   가져간다. 실제로 동시 요청 여섯 개를 던졌더니 셋은 연결조차 못 했다.
//   연결을 유지하면 브라우저가 같은 연결을 재사용하므로 셋으로 충분하다.
//
//   대신 물고 놓지 않는 연결이 소켓을 잡아먹지 않도록 놀고 있으면 끊는다.
#define HTTP_IDLE_MS      3000

typedef struct
{
  uint8_t  sn;
  uint8_t  req_buf[HTTP_REQ_MAX];
  uint16_t req_len;
  uint32_t act_time;      // 마지막으로 뭔가 오간 시각
  bool     keep;          // 이번 응답 뒤에도 연결을 유지할까
} http_conn_t;

static bool        is_init = false;
static http_conn_t conn[HTTP_SN_CNT];
static http_conn_t *p_cur = NULL;     // 지금 처리 중인 연결(응답 전송에 쓴다)

static cmd_t        http_cmd;
static cmd_driver_t http_drv;
static uint8_t      cmd_rsp[CMD_MAX_DATA_LENGTH + 16];
static uint16_t     cmd_rsp_len = 0;

static void httpReset(http_conn_t *p_conn);
static void httpPoll(http_conn_t *p_conn);
static void httpServe(http_conn_t *p_conn);
static bool httpSendFile(const web_file_t *p_file);
static void httpSendStatus(const char *status, const char *body);
static void httpHandleCmd(uint8_t *p_body, uint16_t body_len);




bool netHttpInit(void)
{
  is_init = true;

  for (uint8_t i = 0; i < HTTP_SN_CNT; i++)
  {
    conn[i].sn = (uint8_t)(HTTP_SN_FIRST + i);
    httpReset(&conn[i]);
  }
  return true;
}

//-- 재진입 방지.
//
//   커맨드 처리가 길어질 수 있다. LAN 스캔은 800ms 동안 delay() 로 돌고, 그
//   delay() 는 cliLoopIdle() -> moduleUpdate() 를 부르므로 여기로 다시 들어온다.
//   그대로 두면 처리 중인 연결에 다음 요청을 겹쳐 처리하고 응답이 뒤섞인다.
//   실제로 웹에서 스캔을 누르면 "스캔 중..." 에서 멈췄다.
//
void netHttpUpdate(void)
{
  static bool is_busy = false;

  if (is_init != true || wiznetIsInit() != true)
    return;

  if (is_busy)
    return;

  is_busy = true;
  for (uint8_t i = 0; i < HTTP_SN_CNT; i++)
    httpPoll(&conn[i]);
  is_busy = false;
}

void httpPoll(http_conn_t *p_conn)
{
  uint8_t sn = p_conn->sn;
  uint8_t sr = getSn_SR(sn);

  if (sr == SOCK_CLOSED)
  {
    if (socket(sn, Sn_MR_TCP, NET_HTTP_PORT, SF_IO_NONBLOCK) == sn)
      listen(sn);
    httpReset(p_conn);
    return;
  }

  if (sr == SOCK_ESTABLISHED)
  {
    uint16_t len = getSn_RX_RSR(sn);

    while (len > 0 && p_conn->req_len < HTTP_REQ_MAX)
    {
      uint16_t n = len;

      if (n > (uint16_t)(HTTP_REQ_MAX - p_conn->req_len))
        n = (uint16_t)(HTTP_REQ_MAX - p_conn->req_len);

      if (recv(sn, &p_conn->req_buf[p_conn->req_len], n) != (int32_t)n)
        break;

      p_conn->req_len += n;
      len             -= n;
    }

    if (p_conn->req_len > 0)
    {
      p_conn->act_time = millis();
      httpServe(p_conn);
    }
    else if (millis() - p_conn->act_time >= HTTP_IDLE_MS)
    {
      disconnect(p_conn->sn);
      close(p_conn->sn);
      httpReset(p_conn);
    }
    return;
  }

  if (sr == SOCK_CLOSE_WAIT)
  {
    disconnect(sn);
    close(sn);
    httpReset(p_conn);
  }
}

void httpReset(http_conn_t *p_conn)
{
  p_conn->req_len  = 0;
  p_conn->act_time = millis();
  p_conn->keep     = false;
  cmd_rsp_len      = 0;
}

//-- 요청이 다 왔는지 보고, 다 왔으면 처리한다.
//
//   헤더 끝(\r\n\r\n)이 보여야 시작한다. POST 면 Content-Length 만큼 본문이
//   더 와야 한다. 덜 왔으면 그냥 돌아가서 다음 호출에 이어 받는다.
//
void httpServe(http_conn_t *p_conn)
{
  uint8_t *req_buf = p_conn->req_buf;
  uint16_t req_len = p_conn->req_len;
  char    *p_hdr_end;
  uint16_t hdr_len;
  uint16_t body_len = 0;
  char    *p_len;

  p_cur = p_conn;      // 응답 함수들이 쓸 소켓

  // HTTP/1.1 은 기본이 keep-alive 다. 상대가 close 를 요구할 때만 끊는다.
  p_conn->keep = (strstr((char *)req_buf, "Connection: close") == NULL) &&
                 (strstr((char *)req_buf, "connection: close") == NULL);

  req_buf[req_len < HTTP_REQ_MAX ? req_len : HTTP_REQ_MAX - 1] = 0;

  p_hdr_end = strstr((char *)req_buf, "\r\n\r\n");
  if (p_hdr_end == NULL)
    return;                                   // 헤더가 아직 덜 왔다

  hdr_len = (uint16_t)(p_hdr_end - (char *)req_buf) + 4;

  p_len = strstr((char *)req_buf, "Content-Length:");
  if (p_len != NULL)
  {
    body_len = (uint16_t)atoi(p_len + 15);
    if (req_len < hdr_len + body_len)
      return;                                 // 본문이 아직 덜 왔다
  }

  if (strncmp((char *)req_buf, "POST /cmd", 9) == 0)
  {
    httpHandleCmd(&req_buf[hdr_len], body_len);
  }
  else if (strncmp((char *)req_buf, "GET ", 4) == 0)
  {
    char    *p_path = (char *)&req_buf[4];
    char    *p_sp   = strchr(p_path, ' ');
    bool     found  = false;

    if (p_sp != NULL)
      *p_sp = 0;

    for (uint32_t i = 0; i < web_file_cnt; i++)
    {
      if (strcmp(p_path, web_file_tbl[i].url) == 0)
      {
        httpSendFile(&web_file_tbl[i]);
        found = true;
        break;
      }
    }

    if (found != true)
      httpSendStatus("404 Not Found", "not found");
  }
  else
  {
    httpSendStatus("405 Method Not Allowed", "method not allowed");
  }

  if (p_conn->keep)
  {
    // 처리한 요청만 버리고 연결은 유지한다. 파이프라인으로 뒤이어 온 요청이
    // 버퍼에 남아 있을 수 있으므로 앞으로 당긴다.
    uint16_t used = hdr_len + body_len;

    if (p_conn->req_len > used)
    {
      memmove(p_conn->req_buf, &p_conn->req_buf[used], p_conn->req_len - used);
      p_conn->req_len -= used;
    }
    else
    {
      p_conn->req_len = 0;
    }
    p_conn->act_time = millis();
    return;
  }

  disconnect(p_conn->sn);
  close(p_conn->sn);
  httpReset(p_conn);
}

bool httpSendFile(const web_file_t *p_file)
{
  char     hdr[HTTP_HDR_MAX];
  int      n;
  uint32_t sent = 0;

  n = snprintf(hdr, sizeof(hdr),
               "HTTP/1.1 200 OK\r\n"
               "Content-Type: %s\r\n"
               "Content-Encoding: gzip\r\n"
               "Content-Length: %u\r\n"
               "Cache-Control: no-cache\r\n"
               "Connection: %s\r\n\r\n",
               p_file->mime, (unsigned int)p_file->size,
               p_cur->keep ? "keep-alive" : "close");

  if (send(p_cur->sn, (uint8_t *)hdr, (uint16_t)n) != n)
    return false;

  // 소켓 송신 버퍼보다 크므로 나눠 보낸다.
  while (sent < p_file->size)
  {
    uint32_t remain = p_file->size - sent;
    uint16_t chunk  = (remain > 1024) ? 1024 : (uint16_t)remain;
    int32_t  ret;

    ret = send(p_cur->sn, (uint8_t *)&p_file->data[sent], chunk);
    if (ret <= 0)
      return false;

    sent += (uint32_t)ret;
  }
  return true;
}

void httpSendStatus(const char *status, const char *body)
{
  char hdr[HTTP_HDR_MAX];
  int  n;

  n = snprintf(hdr, sizeof(hdr),
               "HTTP/1.1 %s\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Length: %u\r\n"
               "Connection: %s\r\n\r\n%s",
               status, (unsigned int)strlen(body),
               p_cur->keep ? "keep-alive" : "close", body);

  send(p_cur->sn, (uint8_t *)hdr, (uint16_t)n);
}


//-- POST /cmd
//
//   본문의 cmd 패킷을 그대로 파서에 먹이고, 나온 응답을 본문으로 돌려준다.
//   전송계층만 다를 뿐 CDC/HID/TCP 와 완전히 같은 커맨드 셋이다.
//
static uint16_t rx_idx = 0;
static uint8_t *p_rx_body = NULL;
static uint16_t rx_body_len = 0;

static bool     httpCmdOpen(void *args)      { (void)args; return true; }
static bool     httpCmdClose(void *args)     { (void)args; return true; }
static uint32_t httpCmdAvailable(void *args) { (void)args; return rx_body_len - rx_idx; }
static bool     httpCmdFlush(void *args)     { (void)args; rx_idx = rx_body_len; return true; }

static uint8_t httpCmdRead(void *args)
{
  (void)args;
  if (rx_idx >= rx_body_len)
    return 0;
  return p_rx_body[rx_idx++];
}

//   응답은 바로 보내지 않고 모은다. 헤더에 Content-Length 를 써야 하기 때문이다.
static uint32_t httpCmdWrite(void *args, uint8_t *p_data, uint32_t length)
{
  uint32_t n = length;

  (void)args;

  if (cmd_rsp_len + n > sizeof(cmd_rsp))
    n = sizeof(cmd_rsp) - cmd_rsp_len;

  if (n > 0)
  {
    memcpy(&cmd_rsp[cmd_rsp_len], p_data, n);
    cmd_rsp_len += (uint16_t)n;
  }
  return length;
}

void httpHandleCmd(uint8_t *p_body, uint16_t body_len)
{
  char hdr[HTTP_HDR_MAX];
  int  n;

  p_rx_body   = p_body;
  rx_body_len = body_len;
  rx_idx      = 0;
  cmd_rsp_len = 0;

  if (http_cmd.is_init != true)
  {
    http_drv.open      = httpCmdOpen;
    http_drv.close     = httpCmdClose;
    http_drv.available = httpCmdAvailable;
    http_drv.flush     = httpCmdFlush;
    http_drv.read      = httpCmdRead;
    http_drv.write     = httpCmdWrite;

    cmdInit(&http_cmd, &http_drv);
    cmdOpen(&http_cmd);
  }

  if (cmdReceivePacket(&http_cmd) == true)
    cmdBootProcess(&http_cmd);

  if (cmd_rsp_len == 0)
  {
    httpSendStatus("400 Bad Request", "bad cmd packet");
    return;
  }

  n = snprintf(hdr, sizeof(hdr),
               "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/octet-stream\r\n"
               "Content-Length: %u\r\n"
               "Connection: %s\r\n\r\n",
               (unsigned int)cmd_rsp_len,
               p_cur->keep ? "keep-alive" : "close");

  send(p_cur->sn, (uint8_t *)hdr, (uint16_t)n);
  send(p_cur->sn, cmd_rsp, cmd_rsp_len);
}

#endif
