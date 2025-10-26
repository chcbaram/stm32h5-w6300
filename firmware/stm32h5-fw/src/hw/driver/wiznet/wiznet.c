#include "wiznet.h"
#include "swtimer.h"
#include "cli.h"
#include "rtc.h"
#include "event.h"


#define SOCKET_DHCP           HW_WIZNET_SOCKET_DHCP


#define DHCP_RETRY_COUNT      5
#define DNS_RETRY_COUNT       5

#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

#if CLI_USE(HW_WIZNET)
static void cliCmd(cli_args_t *args);
#endif
static void wiznetPrintInfo(wiz_NetInfo *p_info);
static void wizchip_dhcp_init(void);
static void wizchip_dhcp_assign(void);
static void wizchip_dhcp_conflict(void);
static void wiznetTimerISR(void *arg);
static bool wiznetInitSNTP(void);


static uint8_t memsize[2][8] = {
  {2, 2, 2, 2, 2, 2, 2, 2},
  {2, 2, 2, 2, 2, 2, 2, 2}
};

static uint8_t dhcp_buf[ETHERNET_BUF_MAX_SIZE] = {0,}; 
static uint8_t sntp_buf[ETHERNET_BUF_MAX_SIZE] = {0,}; 
static bool    dhcp_get_ip_flag = false;
static bool    sntp_get_time_flag = false;
static datetime sntp_time;

static bool is_init = false;
static bool is_init_dhcp = false;
static bool is_init_sntp = false;
static bool is_chip_found = true;


static wiz_NetInfo net_info =
    {
        .mac  = {0x00, 0x00, 0x12, 0x34, 0x56, 0x78}, // MAC address
        .ip   = {172,  30,   1,  55},                 // IP address
        .sn   = {255, 255, 255,   0},                 // Subnet Mask
        .gw   = {172,  30,   1, 254},                 // Gateway
        .dns  = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_DHCP                          // DHCP enable/disable
};





bool wiznetInit(void)
{
  bool ret = true;
  uint8_t id_str[6] = {0,};

  
  ret = w6300Init();
  if (!ret)
  {
    logPrintf("[E_] wiznetInit()\n");
    logPrintf("     w6300Init() Fail\n");
    return false;
  }

  if (ctlwizchip(CW_INIT_WIZCHIP, (void *)memsize) == -1)
  {
    ret = false;
  }
  if (ctlwizchip(CW_GET_ID, (void *)id_str) == -1)
  {
    ret = false;
  }


  ctlnetwork(CN_SET_NETINFO, (void *)&net_info);

  if (!w6300IsReady())
  {
    is_chip_found = false;
    ret = false;
  }

  is_init = ret;

  logPrintf("[%s] wiznetInit()\n", ret ? "OK":"E_");
  if (is_init)
  {
    logPrintf("     ID   : %s\n", id_str);
    logPrintf("     Link : %s\n", wiznetIsLink() ? "ON":"OFF");
    wiznetPrintInfo(&net_info);
  }
  else 
  {
    if (!is_chip_found)
    {
      logPrintf("     Chip Not Found\n");
    }
  }

#if CLI_USE(HW_WIZNET)
  cliAdd("wiznet", cliCmd);
#endif
  return ret;
}

bool wiznetIsInit(void)
{
  return is_init;
}

bool wiznetIsLink(void)
{
  bool ret = false;
  uint8_t arg = 0;

  if (!is_init)
    return false;

  if (ctlwizchip(CW_GET_PHYLINK, &arg) == 0)
  {
    ret = arg;
  }

  return ret;
}

bool wiznetDHCP(void)
{
  bool ret = true;

  if (!is_init)
    return false;


  wizchip_dhcp_init();

  if (is_init_dhcp == false)
  {
    swtimer_handle_t timer_ch;
    timer_ch = swtimerGetHandle();
    swtimerSet(timer_ch, 1000, LOOP_TIME, wiznetTimerISR, NULL);
    swtimerStart(timer_ch);
  }
  is_init_dhcp = true;

  logPrintf("[%s] wiznetDHCP()\n", ret ? "OK":"NG");

  return ret;
}

bool wiznetSNTP(void)
{
  bool ret = true;

  if (!is_init)
    return false;

  ret = wiznetInitSNTP();

  logPrintf("[%s] wiznetSNTP()\n", ret ? "OK":"NG");

  return ret;
}

bool wiznetInitSNTP(void)
{
  bool ret = true;
  uint8_t ntp_server[4] = {128, 138, 141, 172};	// time.nist.gov
	//uint8_t ntp_server[4] = {211, 233, 84, 186};	// kr.pool.ntp.org

  if (!is_init)
    return false;

  SNTP_init(HW_WIZNET_SOCKET_SNTP, ntp_server, 40, sntp_buf);	// timezone: Korea, Republic of

  is_init_sntp = true;
  sntp_get_time_flag = false;

  return ret;
}

void wiznetPrintInfo(wiz_NetInfo *p_info)
{
  uint8_t tmp_str[8] = {
      0,
  };
  wiz_NetInfo net_info;


  if (is_init == false)
    return;

  if (p_info != NULL)
  {
    net_info = *p_info;

  }
  ctlnetwork(CN_GET_NETINFO, (void *)&net_info);
  ctlwizchip(CW_GET_ID, (void *)tmp_str);

  logPrintf("[  ] wiznetInfo()\n");

  if (net_info.dhcp == NETINFO_DHCP)
  {
    logPrintf("     %s config : DHCP\n", (char *)tmp_str);
  }
  else
  {
    logPrintf("     %s config : static\n", (char *)tmp_str);
  }

  logPrintf("     MAC          : %02X:%02X:%02X:%02X:%02X:%02X\n", net_info.mac[0], net_info.mac[1], net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
  logPrintf("     IP           : %d.%d.%d.%d\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
  logPrintf("     Subnet Mask  : %d.%d.%d.%d\n", net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
  logPrintf("     Gateway      : %d.%d.%d.%d\n", net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
  logPrintf("     DNS          : %d.%d.%d.%d\n", net_info.dns[0], net_info.dns[1], net_info.dns[2], net_info.dns[3]);
}

bool wiznetIsGetIP(void)
{
  if (is_init_dhcp == false)
    return true;

  return dhcp_get_ip_flag;
}

bool wiznetGetInfo(wiznet_info_t *p_info)
{
  memcpy(p_info->ip,  net_info.ip,  sizeof(net_info.ip));
  memcpy(p_info->dns, net_info.dns, sizeof(net_info.dns));
  memcpy(p_info->gw,  net_info.gw,  sizeof(net_info.gw));
  memcpy(p_info->mac, net_info.mac, sizeof(net_info.mac));
  memcpy(p_info->sn,  net_info.sn,  sizeof(net_info.sn));

  return true;
}

void wiznetUpdateDHCP(void)
{
  static bool pre_link = false;
  bool cur_link;


  if (is_init_dhcp == false)
    return;

  cur_link = wiznetIsLink();  
  if (cur_link == true && pre_link == false)
  {
    DHCP_init(SOCKET_DHCP, dhcp_buf);    
    dhcp_get_ip_flag = false;
    logPrintf("[  ] DHCP_init()\n");    
  }
  pre_link = cur_link;


  // Assigned IP through DHCP
  //
  if (cur_link == true && net_info.dhcp == NETINFO_DHCP)
  {
    static uint8_t dhcp_state = 0;
    static uint8_t dhcp_retry = 0;

    dhcp_state = DHCP_run();

    switch(dhcp_state)
    {
      case DHCP_IP_LEASED:
        if (dhcp_get_ip_flag == false)
        {
          logPrintf("[OK] DHCP Success\n");
          wiznetPrintInfo(&net_info);
          logPrintf("     DHCP Leased Time : %ld Sec\n", getDHCPLeasetime());          
          dhcp_get_ip_flag = true;
          eventPub(EVENT_WIZ_PHY_DHCP, 0);
        }
        break;

      case DHCP_FAILED:
        dhcp_retry++;

        if (dhcp_retry >= DHCP_RETRY_COUNT)
        {
          dhcp_retry = 0;
          dhcp_get_ip_flag = false;
          DHCP_stop();

          ctlnetwork(CN_SET_NETINFO, (void *)&net_info);

          logPrintf("[NG] DHCP_FAILED\n");
          eventPub(EVENT_WIZ_PHY_DHCP, 2);
        }
        else
        {
          // logPrintf("[  ] DHCP RETRY %d\n", dhcp_retry);
          eventPub(EVENT_WIZ_PHY_DHCP, 1);
        }
        break;


      case DHCP_RUNNING:
      case DHCP_IP_ASSIGN:
      case DHCP_IP_CHANGED:
      case DHCP_STOPPED:
      default:
        break;
    }
  }
}

void wiznetUpdateSNTP(void)
{
  static bool pre_link = false;
  bool cur_link;

  if (is_init_sntp != true)
    return;

  cur_link = wiznetIsLink();  
  if (cur_link == true && pre_link == false)
  {
    wiznetInitSNTP();    
    sntp_get_time_flag = false;
    logPrintf("[  ] SNTP_init()\n");    
  }
  pre_link = cur_link;


  if (dhcp_get_ip_flag != true)
    return;

  if (sntp_get_time_flag == true)
    return;


  if (SNTP_run(&sntp_time) == true)
  {
    logPrintf("[OK] SNTP\n");
    logPrintf("     %d-%d-%d, %02d:%02d:%02d\n", 
        sntp_time.yy, sntp_time.mo, sntp_time.dd, sntp_time.hh, sntp_time.mm, sntp_time.ss);

    sntp_get_time_flag = true;

    rtc_time_t rtc_time;
    rtc_date_t rtc_date;

    rtc_date.year  = sntp_time.yy % 100;
    rtc_date.month = sntp_time.mo;
    rtc_date.day   = sntp_time.dd;
    rtcSetDate(&rtc_date);

    rtc_time.hours   = sntp_time.hh;
    rtc_time.minutes = sntp_time.mm;
    rtc_time.seconds = sntp_time.ss; 
    rtcSetTime(&rtc_time);

    eventPub(EVENT_WIZ_PHY_SNTP, 1);
  }
}

void wiznetUpdateLink(void)
{
  static bool first_run = true;
  static bool linked = false;
  bool cur_linked;

  if (is_init == false)
    return;

  if (first_run)
  {
    first_run = false;

    linked = !wiznetIsLink();
  }

  cur_linked = wiznetIsLink();
  if (cur_linked != linked)
  {
    eventPub(EVENT_WIZ_PHY_LINK, cur_linked);
  }
  linked = cur_linked;
}

void wiznetUpdate(void)
{
  wiznetUpdateDHCP();
  wiznetUpdateSNTP();
  wiznetUpdateLink();
}

void wiznetTimerISR(void *arg)
{
  DHCP_time_handler();
  DNS_time_handler();
}

void wizchip_dhcp_init(void)
{
  DHCP_init(SOCKET_DHCP, dhcp_buf);
  reg_dhcp_cbfunc(wizchip_dhcp_assign, wizchip_dhcp_assign, wizchip_dhcp_conflict);
}

static void wizchip_dhcp_assign(void)
{
  getIPfromDHCP(net_info.ip);
  getGWfromDHCP(net_info.gw);
  getSNfromDHCP(net_info.sn);
  getDNSfromDHCP(net_info.dns);

  net_info.dhcp = NETINFO_DHCP;

  /* Network initialize */
  ctlnetwork(CN_SET_NETINFO, (void *)&net_info);
}

static void wizchip_dhcp_conflict(void)
{
  logPrintf("     Conflict IP from DHCP\n");
}

#if CLI_USE(HW_WIZNET)
void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    cliPrintf("is_init   \t: %d\n", is_init ? "True":"False");
    cliPrintf("is_found  \t: %s\n", is_chip_found ? "True":"False");
    cliPrintf("is_dhcp   \t: %d\n", is_init_dhcp ? "True":"False");
    cliPrintf("is_ip_get \t: %s\n", wiznetIsGetIP() ? "True":"False");

    wiznetPrintInfo(&net_info);
    ret = true;
  }  

  if (ret != true)
  {
    cliPrintf("wiznet info\n");
  }
}
#endif