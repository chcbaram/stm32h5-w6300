#include "net_disc.h"

#ifdef _USE_HW_WIZNET


//-- LAN 안의 보드 찾기.
//
//   브라우저는 원시 네트워크 스캔을 할 수 없다. 그래서 USB 로 붙은 보드가
//   대신 훑고 목록만 돌려준다(14-roadmap.md B).
//
//   방식은 UDP 브로드캐스트 비컨이다. ARP/핑 스윕(254개 주소)과 달리 패킷
//   하나면 끝나고, 우리 보드끼리만 쓰는 규약이라 이름·버전·모드까지 한 번에
//   받아올 수 있다.
//
//   소켓은 항상 열어둔다. 다른 보드가 물어보면 언제든 답해야 하기 때문이다.
//
#define DISC_SN               4         // 0=CLI TCP, 1=DHCP, 2=SNTP, 3=예약

static bool         is_init  = false;
static net_beacon_t rx_buf;

static bool         is_scan  = false;
static net_beacon_t *p_scan_list = NULL;
static uint8_t       scan_max = 0;
static uint8_t       scan_cnt = 0;

static void netDiscOpen(void);
static void netDiscFillSelf(net_beacon_t *p_beacon, uint32_t magic);
static void netDiscAddFound(const net_beacon_t *p_beacon);

#if CLI_USE(HW_WIZNET)
static void cliNetDisc(cli_args_t *args);
#endif




bool netDiscInit(void)
{
  is_init = true;

#if CLI_USE(HW_WIZNET)
  cliAdd("scan", cliNetDisc);
#endif
  return true;
}

//-- 들어온 비컨을 처리한다.
//
//   질의(BRDQ)면 답하고, 응답(BRDR)이면 스캔 중일 때만 목록에 담는다.
//   ap 모듈 update 에서 계속 불린다.
//
void netDiscUpdate(void)
{
  uint8_t  addr[4];
  uint16_t port;
  uint8_t  addr_len = 4;
  int32_t  len;

  if (is_init != true || wiznetIsInit() != true)
    return;

  netDiscOpen();

  if (getSn_RX_RSR(DISC_SN) == 0)
    return;

  len = recvfrom(DISC_SN, (uint8_t *)&rx_buf, sizeof(rx_buf), addr, &port, &addr_len);
  if (len < (int32_t)sizeof(net_beacon_t))
    return;

  if (rx_buf.ver != NET_DISC_VER)
    return;

  if (rx_buf.magic == NET_DISC_MAGIC_REQ)
  {
    net_beacon_t rsp;

    netDiscFillSelf(&rsp, NET_DISC_MAGIC_RSP);
    sendto(DISC_SN, (uint8_t *)&rsp, sizeof(rsp), addr, port, 4);
  }
  else if (rx_buf.magic == NET_DISC_MAGIC_RSP)
  {
    netDiscAddFound(&rx_buf);
  }
}

uint8_t netDiscScan(net_beacon_t *p_list, uint8_t list_max, uint32_t timeout_ms)
{
  net_beacon_t  req;
  wiznet_info_t info;
  uint8_t       bcast[4];
  uint32_t      pre_time;

  if (p_list == NULL || list_max == 0)
    return 0;

  // 이 보드부터 담는다. 칩이 자기 브로드캐스트를 되받는다는 보장이 없다.
  netDiscFillSelf(&p_list[0], NET_DISC_MAGIC_RSP);

  p_scan_list = p_list;
  scan_max    = list_max;
  scan_cnt    = 1;

  if (is_init != true || wiznetIsInit() != true || wiznetIsLink() != true)
    return scan_cnt;

  netDiscOpen();

  // 서브넷 브로드캐스트로 보낸다. 255.255.255.255 보다 라우팅 사고가 적다.
  wiznetGetInfo(&info);
  for (int i = 0; i < 4; i++)
    bcast[i] = info.ip[i] | (uint8_t)(~info.sn[i]);

  netDiscFillSelf(&req, NET_DISC_MAGIC_REQ);

  is_scan = true;
  sendto(DISC_SN, (uint8_t *)&req, sizeof(req), bcast, NET_DISC_PORT, 4);

  pre_time = millis();
  while (millis() - pre_time < timeout_ms)
  {
    netDiscUpdate();
    delay(2);                   // delay() 안에서 USB 가 계속 돈다
    if (scan_cnt >= scan_max)
      break;
  }
  is_scan = false;

  return scan_cnt;
}

void netDiscOpen(void)
{
  if (getSn_SR(DISC_SN) == SOCK_UDP)
    return;

  close(DISC_SN);
  socket(DISC_SN, Sn_MR_UDP4, NET_DISC_PORT, 0);
}

void netDiscFillSelf(net_beacon_t *p_beacon, uint32_t magic)
{
  wiznet_info_t info;

  memset(p_beacon, 0, sizeof(net_beacon_t));
  p_beacon->magic = magic;
  p_beacon->ver   = NET_DISC_VER;
  p_beacon->mode  = HW_DEV_MODE;

  if (wiznetGetInfo(&info) == true)
  {
    memcpy(p_beacon->ip,  info.ip,  sizeof(p_beacon->ip));
    memcpy(p_beacon->mac, info.mac, sizeof(p_beacon->mac));
  }

  // 플래시의 문자열은 NUL 보장이 없다. strncpy 로 길이를 잘라 담는다.
  strncpy(p_beacon->name,    _DEF_BOARD_NAME,        sizeof(p_beacon->name) - 1);
  strncpy(p_beacon->version, _DEF_FIRMWATRE_VERSION, sizeof(p_beacon->version) - 1);
}

//   같은 IP 가 두 번 들어오면 무시한다. 브로드캐스트가 되돌아오거나 응답이
//   중복될 수 있다.
void netDiscAddFound(const net_beacon_t *p_beacon)
{
  if (is_scan != true || p_scan_list == NULL || scan_cnt >= scan_max)
    return;

  for (uint8_t i = 0; i < scan_cnt; i++)
  {
    if (memcmp(p_scan_list[i].ip, p_beacon->ip, 4) == 0)
      return;
  }

  p_scan_list[scan_cnt] = *p_beacon;
  scan_cnt++;
}


#if CLI_USE(HW_WIZNET)
void cliNetDisc(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 0 || (args->argc == 1 && args->isStr(0, "run")))
  {
    net_beacon_t list[NET_DISC_LIST_MAX];
    uint8_t      n;
    uint32_t     t0 = millis();

    cliPrintf("LAN 스캔...\n");
    n = netDiscScan(list, NET_DISC_LIST_MAX, 800);
    cliPrintf("%d 대 (%d ms)\n\n", n, (int)(millis() - t0));
    cliPrintf("IP               MODE  NAME                     VERSION\n");

    for (uint8_t i = 0; i < n; i++)
    {
      char ip_str[16];

      snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
               list[i].ip[0], list[i].ip[1], list[i].ip[2], list[i].ip[3]);

      // 플래시/네트워크에서 온 문자열은 NUL 보장이 없다. 폭을 잘라 찍는다.
      cliPrintf("%-16s %-5s %-24.*s %.*s\n",
                ip_str,
                list[i].mode == HW_DEV_MODE_BOOT ? "BOOT" : "APP",
                NET_DISC_NAME_LEN, list[i].name,
                NET_DISC_VER_LEN,  list[i].version);
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("scan\n");
    cliPrintf("scan run\n");
  }
}
#endif

#endif
