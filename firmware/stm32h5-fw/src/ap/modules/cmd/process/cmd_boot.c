#include "cmd_task.h"

#ifdef _USE_HW_CMD


//-- 부트로더 커맨드 셋.
//
//   CDC / HID / (향후) 이더넷이 모두 이 하나를 공유한다. 채널 드라이버만 다르다.
//
#define BOOT_CMD_INFO             0x0000
#define BOOT_CMD_VERSION          0x0001
#define BOOT_CMD_FW_BEGIN         0x0002
#define BOOT_CMD_FW_ERASE         0x0003
#define BOOT_CMD_FW_WRITE         0x0004
#define BOOT_CMD_FW_READ          0x0005
#define BOOT_CMD_FW_END           0x0006
#define BOOT_CMD_FW_VERIFY        0x0007
#define BOOT_CMD_FW_UPDATE        0x0008
#define BOOT_CMD_FW_JUMP          0x0009
#define BOOT_CMD_LOG_COUNT        0x000A
#define BOOT_CMD_LOG_READ         0x000B
#define BOOT_CMD_CLI              0x0010
#define BOOT_CMD_CLI_MORE         0x0011
#define BOOT_CMD_RTC              0x0012
#define BOOT_CMD_NET              0x0013
#define BOOT_CMD_NET_SCAN         0x0014

#define BOOT_RTC_OP_GET           0
#define BOOT_RTC_OP_SET           1


//-- 호스트가 연결 직후 가장 먼저 부르는 커맨드의 응답.
//
//   부트로더와 앱이 같은 VID/PID 로 열거되므로 USB 만으로는 어느 쪽인지 알 수 없다.
//   mode 와 실행 중인 펌웨어의 이름/버전을 여기서 알려준다. 웹/툴은 이 값으로
//   보여줄 항목을 결정한다.
//
typedef struct
{
  uint32_t magic;
  uint32_t mode;              // HW_DEV_MODE_BOOT / HW_DEV_MODE_APP
  uint32_t boot_addr;
  uint32_t firm_addr;
  uint32_t firm_size;
  uint32_t slot_size;
  uint32_t slot_max;
  uint32_t family_id;
  char     name[32];          // 지금 실행 중인 쪽의 이름
  char     version[32];       // 지금 실행 중인 쪽의 버전
} __attribute__((packed)) boot_info_t;

typedef struct
{
  uint8_t  valid;
  uint8_t  index;
  uint16_t rsv;
  uint32_t addr;
  uint32_t seq;
  uint32_t fw_size;
  uint32_t fw_crc;
  char     name[32];
  char     version[32];
} __attribute__((packed)) boot_ver_item_t;

//-- 네트워크 상태. 앱에만 있다(부트로더는 이더넷을 올리지 않는다).
//
//   호스트가 보드의 IP 를 알아야 보드 자체 웹서버로 넘어갈 수 있다. 지금은
//   조회만 하고, LAN 스캔은 여기에 커맨드를 더해 붙인다.
//
typedef struct
{
  uint8_t  is_valid;          // 0 이면 이 보드에 이더넷이 없다
  uint8_t  is_link;
  uint8_t  is_dhcp;
  uint8_t  is_ip_get;
  uint8_t  mac[6];
  uint8_t  rsv[2];
  uint8_t  ip[4];
  uint8_t  sn[4];
  uint8_t  gw[4];
  uint8_t  dns[4];
} __attribute__((packed)) boot_net_t;

typedef struct
{
  boot_ver_item_t firm;
  boot_ver_item_t slot[FLASH_SLOT_MAX];
  int8_t          write_slot;
  int8_t          pending_slot;
  int8_t          rollback_slot;
  int8_t          rsv;
} __attribute__((packed)) boot_version_t;


static bool     is_busy   = false;
static int8_t   wr_slot   = -1;
static uint32_t wr_length = 0;
static uint32_t wr_index  = 0;


bool cmdBootIsBusy(void)
{
  return is_busy;
}

static void bootFillItem(boot_ver_item_t *p_item, uint8_t index, uint32_t addr, bool valid,
                         uint32_t seq, uint32_t fw_size, uint32_t fw_crc)
{
  firm_ver_t ver;

  memset(p_item, 0, sizeof(boot_ver_item_t));
  p_item->valid   = valid ? 1 : 0;
  p_item->index   = index;
  p_item->addr    = addr;
  p_item->seq     = seq;
  p_item->fw_size = fw_size;
  p_item->fw_crc  = fw_crc;

  if (!valid)
    return;

  flashRead(addr + FLASH_SIZE_TAG + FLASH_SIZE_VEC, (uint8_t *)&ver, sizeof(ver));
  if (ver.magic_number == VERSION_MAGIC_NUMBER)
  {
    memcpy(p_item->name,    ver.name_str,    sizeof(p_item->name));
    memcpy(p_item->version, ver.version_str, sizeof(p_item->version));
  }
}


bool cmdBootProcess(cmd_t *p_cmd)
{
  uint16_t cmd      = p_cmd->packet.cmd;
  uint8_t *p_data   = p_cmd->packet.data;
  uint16_t length   = p_cmd->packet.length;
  uint16_t err_code = CMD_OK;


  switch (cmd)
  {
    case BOOT_CMD_INFO:
    {
      boot_info_t info;

      memset(&info, 0, sizeof(info));
      info.magic     = VERSION_MAGIC_NUMBER;
      info.mode      = HW_DEV_MODE;
      info.boot_addr = FLASH_ADDR_BOOT;
      info.firm_addr = FLASH_ADDR_FIRM;
      info.firm_size = FLASH_SIZE_FIRM;
      info.slot_size = FLASH_SIZE_SLOT;
      info.slot_max  = FLASH_SLOT_MAX;
      info.family_id = BOARD_UF2_FAMILY_ID;

      // 자기 이름/버전. memcpy 로 고정 길이를 복사하면 문자열 리터럴 끝을 넘어
      // 읽으므로 strncpy 를 쓴다(구조체는 위에서 0 으로 초기화했다).
      strncpy(info.name,    _DEF_BOARD_NAME,        sizeof(info.name) - 1);
      strncpy(info.version, _DEF_FIRMWATRE_VERSION, sizeof(info.version) - 1);

      cmdSendResp(p_cmd, cmd, CMD_OK, (uint8_t *)&info, sizeof(info));
      break;
    }

    case BOOT_CMD_VERSION:
    {
      static boot_version_t ver;
      boot_slot_info_t info;

      bootGetFirmInfo(&info);
      bootFillItem(&ver.firm, 0xFF, FLASH_ADDR_FIRM, info.valid,
                   info.seq, info.fw_size, info.fw_crc);

      for (uint8_t i = 0; i < FLASH_SLOT_MAX; i++)
      {
        bootGetSlotInfo(i, &info);
        bootFillItem(&ver.slot[i], i, info.addr, info.valid,
                     info.seq, info.fw_size, info.fw_crc);
      }

      ver.write_slot    = bootGetWriteSlot();
      ver.pending_slot  = bootGetPendingSlot();
      ver.rollback_slot = bootGetRollbackSlot();
      ver.rsv           = 0;

      cmdSendResp(p_cmd, cmd, CMD_OK, (uint8_t *)&ver, sizeof(ver));
      break;
    }

    //-- 전송 시작. 쓸 슬롯을 정하고 크기를 받는다.
    case BOOT_CMD_FW_BEGIN:
    {
      uint32_t fw_size = 0;

      if (length >= 4)
        memcpy(&fw_size, &p_data[0], 4);

      wr_slot = bootGetWriteSlot();
      if (wr_slot < 0)
      {
        err_code = ERR_BOOT_WRONG_RANGE;
      }
      else if (fw_size == 0 || fw_size > (FLASH_SIZE_FIRM - FLASH_SIZE_TAG))
      {
        err_code = ERR_BOOT_TAG_SIZE;
      }
      else
      {
        wr_length = fw_size;
        wr_index  = 0;
        is_busy   = true;
        logPrintf("[  ] cmd fw begin slot%d %d KB\n", wr_slot, (int)(fw_size/1024));
      }
      {
        uint8_t resp = (wr_slot < 0) ? 0xFF : (uint8_t)wr_slot;
        cmdSendResp(p_cmd, cmd, err_code, &resp, 1);
      }
      break;
    }

    //-- 슬롯 소거. 태그 영역까지 포함해 필요한 만큼만 지운다.
    case BOOT_CMD_FW_ERASE:
    {
      if (wr_slot < 0)
      {
        err_code = ERR_BOOT_WRONG_CMD;
      }
      else
      {
        uint32_t addr = bootGetSlotAddr((uint8_t)wr_slot);

        if (flashErase(addr, FLASH_SIZE_TAG + wr_length) != true)
          err_code = ERR_BOOT_FLASH_ERASE;
      }
      cmdSendResp(p_cmd, cmd, err_code, NULL, 0);
      break;
    }

    //-- 데이터 기록. [0:3] offset, [4:] data
    case BOOT_CMD_FW_WRITE:
    {
      uint32_t offset = 0;

      if (length < 4 || wr_slot < 0)
      {
        err_code = ERR_BOOT_WRONG_CMD;
      }
      else
      {
        uint32_t n = length - 4;

        memcpy(&offset, &p_data[0], 4);

        if ((offset + n) > wr_length || (offset % 16) != 0)
        {
          err_code = ERR_BOOT_WRONG_RANGE;
        }
        else
        {
          uint32_t addr = bootGetSlotAddr((uint8_t)wr_slot) + FLASH_SIZE_TAG + offset;

          if (flashWrite(addr, &p_data[4], n) != true)
            err_code = ERR_BOOT_FLASH_WRITE;
          else if ((offset + n) > wr_index)
            wr_index = offset + n;
        }
      }
      cmdSendResp(p_cmd, cmd, err_code, NULL, 0);
      break;
    }

    case BOOT_CMD_FW_READ:
    {
      static uint8_t rd_buf[256];
      uint32_t offset = 0;
      uint32_t n = 0;

      if (length >= 8)
      {
        memcpy(&offset, &p_data[0], 4);
        memcpy(&n,      &p_data[4], 4);
      }
      if (n > sizeof(rd_buf) || wr_slot < 0)
      {
        err_code = ERR_BOOT_WRONG_RANGE;
        cmdSendResp(p_cmd, cmd, err_code, NULL, 0);
      }
      else
      {
        uint32_t addr = bootGetSlotAddr((uint8_t)wr_slot) + FLASH_SIZE_TAG + offset;

        if (flashRead(addr, rd_buf, n) != true)
        {
          err_code = ERR_BOOT_FLASH_READ;
          cmdSendResp(p_cmd, cmd, err_code, NULL, 0);
        }
        else
        {
          cmdSendResp(p_cmd, cmd, CMD_OK, rd_buf, n);
        }
      }
      break;
    }

    //-- 전송 종료. 태그를 기록해 슬롯을 확정한다.
    //   기록 순서는 UF2 경로와 동일해야 한다(전원 손실 대응).
    case BOOT_CMD_FW_END:
    {
      if (wr_slot < 0 || wr_index == 0)
      {
        err_code = ERR_BOOT_WRONG_CMD;
      }
      else
      {
        uint32_t    base = bootGetSlotAddr((uint8_t)wr_slot);
        uint8_t     buf[16] __attribute__((aligned(16)));
        uint8_t     rd[256];
        uint16_t    crc = 0;
        firm_tag_t  tag;
        boot_slot_t slot;

        for (uint32_t i = 0; i < wr_index; )
        {
          uint32_t n = wr_index - i;
          if (n > sizeof(rd)) n = sizeof(rd);
          if (flashRead(base + FLASH_SIZE_TAG + i, rd, n) != true) { err_code = ERR_BOOT_FLASH_READ; break; }
          crc = utilCalcCRC(crc, rd, n);
          i += n;
        }

        if (err_code == CMD_OK)
        {
          tag.magic_number = TAG_MAGIC_NUMBER;
          tag.fw_addr      = FLASH_SIZE_TAG;
          tag.fw_size      = wr_index;
          tag.fw_crc       = crc;
          tag.tag_crc      = 0;

          slot.magic = BOOT_SLOT_MAGIC;
          slot.seq   = bootGetNextSeq();
          slot.flags = BOOT_SLOT_FLAG_NONE;
          slot.crc   = utilCalcCRC(0, (uint8_t *)&slot, 12);

          memset(buf, 0xFF, sizeof(buf)); memcpy(buf, &slot, sizeof(slot));
          if (flashWrite(base + BOOT_SLOT_TAG_OFFSET, buf, 16) != true) err_code = ERR_BOOT_FLASH_WRITE;

          tag.tag_crc = utilCalcCRC(0, (uint8_t *)&tag, 16);
          memset(buf, 0xFF, sizeof(buf)); memcpy(buf, &tag.tag_crc, 4);
          if (err_code == CMD_OK && flashWrite(base + 0x10, buf, 16) != true) err_code = ERR_BOOT_FLASH_WRITE;

          memset(buf, 0xFF, sizeof(buf)); memcpy(buf, &tag, 16);
          if (err_code == CMD_OK && flashWrite(base + 0x00, buf, 16) != true) err_code = ERR_BOOT_FLASH_WRITE;

          if (err_code == CMD_OK)
            err_code = bootVerifySlot(base);

          logPrintf("[%s] cmd fw end slot%d size=%d crc=0x%04X seq=%d\n",
                    err_code == CMD_OK ? "OK" : "E_", wr_slot,
                    (int)wr_index, crc, (int)slot.seq);
        }
        is_busy = false;
      }
      cmdSendResp(p_cmd, cmd, err_code, NULL, 0);
      break;
    }

    case BOOT_CMD_FW_VERIFY:
    {
      int8_t n = (length >= 1) ? (int8_t)p_data[0] : wr_slot;

      if (n < 0 || n >= FLASH_SLOT_MAX)
        err_code = ERR_BOOT_WRONG_RANGE;
      else
        err_code = bootVerifySlot(bootGetSlotAddr((uint8_t)n));

      cmdSendResp(p_cmd, cmd, err_code, NULL, 0);
      break;
    }

    //-- 슬롯 -> FIRM 적용.
    //
    //   부트로더는 직접 복사하고 점프한다. 앱은 자기가 실행 중인 뱅크1 을
    //   지울 수 없으므로 적용 요청만 남기고 리셋한다. 다음 부팅에서
    //   부트로더가 bootGetPendingSlot() 을 보고 같은 일을 한다.
    //
    case BOOT_CMD_FW_UPDATE:
    {
#if HW_DEV_MODE == HW_DEV_MODE_BOOT
      int8_t n = bootGetPendingSlot();

      cmdSendResp(p_cmd, cmd, CMD_OK, NULL, 0);
      delay(100);                       // 응답이 호스트에 전달될 시간을 준다

      if (n < 0)
        n = wr_slot;

      if (n >= 0)
      {
        boot_slot_info_t firm, info;

        bootGetFirmInfo(&firm);
        bootGetSlotInfo((uint8_t)n, &info);

        usbDisconnect();
        err_code = bootApplySlot((uint8_t)n);
        bootLogWrite(err_code == OK ? BOOT_EVT_UPDATE : BOOT_EVT_VERIFY_FAIL,
                     n, firm.fw_crc, info.fw_crc, 0);

        logPrintf("[%s] cmd fw update slot%d err 0x%04X\n",
                  err_code == OK ? "OK" : "E_", n, err_code);

        if (err_code == OK)
        {
          delay(300);
          bootJumpFirm();
        }
        usbConnect();
      }
#else
      //   슬롯에 받아둔 것이 지금 FIRM 과 같으면 부트로더가 할 일이 없다.
      //   그대로 리셋하면 아무 일도 없이 재부팅만 하고, 부트 이벤트 로그에도
      //   남지 않아 호스트는 "적용됐다" 고 잘못 안다. 여기서 걸러 알려준다.
      if (bootGetPendingSlot() < 0)
        err_code = ERR_BOOT_NO_PENDING;

      cmdSendResp(p_cmd, cmd, err_code, NULL, 0);

      if (err_code == CMD_OK)
      {
        logPrintf("[  ] cmd fw update -> reset\n");
        delay(300);                     // 응답이 호스트에 전달될 시간을 준다
        resetToUpdate();
      }
      else
      {
        logPrintf("[  ] cmd fw update : 적용할 것이 없다(FIRM 과 동일)\n");
      }
#endif
      break;
    }

    //-- 부트로더에서는 앱으로 점프, 앱에서는 부트로더로 재진입.
    //
    //   앱 쪽 동작이 있어야 웹페이지에서 리셋 버튼을 물리적으로 누르지 않고도
    //   부트로더로 넘어갈 수 있다.
    //
    case BOOT_CMD_FW_JUMP:
      cmdSendResp(p_cmd, cmd, CMD_OK, NULL, 0);
      delay(100);
#if HW_DEV_MODE == HW_DEV_MODE_BOOT
      usbDisconnect();
      bootJumpFirm();
      usbConnect();
#else
      delay(200);
      resetToBoot();
#endif
      break;

    case BOOT_CMD_LOG_COUNT:
    {
      uint16_t n = bootLogGetCount();
      cmdSendResp(p_cmd, cmd, CMD_OK, (uint8_t *)&n, sizeof(n));
      break;
    }

    case BOOT_CMD_LOG_READ:
    {
      boot_log_t log;
      uint16_t   idx = 0;

      if (length >= 2)
        memcpy(&idx, &p_data[0], 2);

      if (bootLogRead(idx, &log) != true)
        cmdSendResp(p_cmd, cmd, ERR_BOOT_WRONG_RANGE, NULL, 0);
      else
        cmdSendResp(p_cmd, cmd, CMD_OK, (uint8_t *)&log, sizeof(log));
      break;
    }

    //-- CLI 한 줄을 실행하고 출력을 돌려준다.
    //
    //   cli.c 는 가상 UART 채널(HW_UART_CH_CMD)에서 읽으므로 아무것도 모른다.
    //   전송이 HID 든 CDC 든 (향후) 네트워크든 동일하게 동작한다.
    //
    //   출력은 한 패킷에 다 들어가지 않는다(`log` 덤프는 수 KB 다). 조각내서
    //   보내고, 호스트는 more 플래그가 0 이 될 때까지 BOOT_CMD_CLI_MORE 로
    //   나머지를 가져간다.
    //
    //   응답 형식 : [0] = more(0/1),  [1:] = 출력 텍스트
    //
    case BOOT_CMD_CLI:
    {
      uint8_t prev_port = cliGetPort();

      drvCliPutLine(p_cmd, p_data, length);

      // cli.c 는 '현재 열린 포트' 로 출력한다. 잠시 가상 CLI 채널로 돌려놓아야
      // 출력이 USART1 이 아니라 우리 버퍼로 모인다.
      //
      // cli_mgr 이 같은 moduleUpdate 안에서 cliMain() 을 또 부르면 재진입이 되고,
      // 포트를 원래대로 되돌려 버리기까지 한다. 그동안 통째로 꺼둔다.
      cliMgrEnable(false);
      cliOpen(HW_UART_CH_CMD, 0);

      {
        uint32_t pre_time = millis();

        while (millis() - pre_time < 300)
        {
          cliMain();
          if (uartAvailable(HW_UART_CH_CMD) == 0)
            break;
        }
        cliMain();      // 프롬프트까지 뱉게 한 번 더
      }

      cliOpen(prev_port, 0);
      cliMgrEnable(true);

      drvCliEndOut();
    }
    /* fall through - 첫 조각을 바로 보낸다 */

    case BOOT_CMD_CLI_MORE:
    {
      static uint8_t resp[CLI_CHUNK_SIZE + 1];
      uint8_t  *p_out;
      uint32_t  out_len;

      out_len = drvCliGetOut(&p_out, CLI_CHUNK_SIZE);

      resp[0] = drvCliHasMore() ? 1 : 0;
      memcpy(&resp[1], p_out, out_len);

      cmdSendResp(p_cmd, cmd, CMD_OK, resp, (uint16_t)(out_len + 1));

      if (resp[0] == 0)
        drvCliClearOut();
      break;
    }

    //-- 보드 시각 조회/설정.
    //
    //   op 하나로 읽기와 쓰기를 겸하고, 어느 쪽이든 **현재 값을 되돌려준다.**
    //   쓰기 직후 확인이 한 번에 끝난다(convex 의 CMD_RTC 와 같은 형태).
    //
    //   값은 uint32 epoch 다. 다만 RTC 는 지역시(SNTP 가 KST 로 맞춘다)를 담고
    //   있으므로, "달력 필드를 UTC 로 간주해 만든 epoch" 이라는 약속이다.
    //   호스트는 표시할 때 getUTC*() 를 쓰고, 보낼 때 자기 지역시를 같은 방식으로
    //   인코딩한다. 그러면 화면에 보이는 값이 보드의 벽시계와 정확히 일치한다.
    //
    //   요청 : [0] op, (SET 이면) [1..4] epoch
    //   응답 : [0..3] 현재 epoch (0 = 시각 모름)
    //
    case BOOT_CMD_RTC:
    {
#ifdef _USE_HW_RTC
      uint32_t epoch = 0;

      if (length >= 5 && p_data[0] == BOOT_RTC_OP_SET)
      {
        memcpy(&epoch, &p_data[1], 4);
        if (rtcSetEpoch(epoch) != true)
          err_code = ERR_BOOT_WRONG_RANGE;
      }

      epoch = rtcGetEpoch();
      cmdSendResp(p_cmd, cmd, err_code, (uint8_t *)&epoch, sizeof(epoch));
#else
      cmdSendResp(p_cmd, cmd, ERR_BOOT_WRONG_CMD, NULL, 0);
#endif
      break;
    }

    //-- 네트워크 상태 조회.
    //
    //   이더넷이 없는 쪽(부트로더)도 응답은 한다. is_valid = 0 으로 알려주면
    //   호스트가 "지원 안 함" 과 "통신 실패" 를 구분할 수 있다.
    //
    case BOOT_CMD_NET:
    {
      boot_net_t net;

      memset(&net, 0, sizeof(net));

#ifdef _USE_HW_WIZNET
      {
        wiznet_info_t info;

        net.is_valid  = 1;
        net.is_link   = wiznetIsLink()  ? 1 : 0;
        net.is_ip_get = wiznetIsGetIP() ? 1 : 0;

        if (wiznetGetInfo(&info) == true)
        {
          net.is_dhcp = info.dhcp ? 1 : 0;
          memcpy(net.mac, info.mac, sizeof(net.mac));
          memcpy(net.ip,  info.ip,  sizeof(net.ip));
          memcpy(net.sn,  info.sn,  sizeof(net.sn));
          memcpy(net.gw,  info.gw,  sizeof(net.gw));
          memcpy(net.dns, info.dns, sizeof(net.dns));
        }
      }
#endif
      cmdSendResp(p_cmd, cmd, CMD_OK, (uint8_t *)&net, sizeof(net));
      break;
    }

    //-- LAN 스캔.
    //
    //   브라우저는 원시 네트워크 스캔을 할 수 없다. 그래서 USB 로 붙은 보드가
    //   대신 훑는다(14-roadmap.md B). 800ms 쯤 걸리므로 호스트는 타임아웃을
    //   넉넉히 잡아야 한다.
    //
    //   응답 : [0] 개수, [1..3] 예약, 그 뒤로 net_beacon_t 배열
    //
    case BOOT_CMD_NET_SCAN:
    {
#ifdef _USE_HW_WIZNET
      static uint8_t resp[4 + NET_DISC_LIST_MAX * sizeof(net_beacon_t)];
      uint8_t n;

      memset(resp, 0, sizeof(resp));
      n = netDiscScan((net_beacon_t *)&resp[4], NET_DISC_LIST_MAX, 800);
      resp[0] = n;

      cmdSendResp(p_cmd, cmd, CMD_OK, resp,
                  (uint16_t)(4 + n * sizeof(net_beacon_t)));
#else
      cmdSendResp(p_cmd, cmd, ERR_BOOT_WRONG_CMD, NULL, 0);
#endif
      break;
    }

    default:
      cmdSendResp(p_cmd, cmd, ERR_BOOT_WRONG_CMD, NULL, 0);
      break;
  }

  return true;
}

#endif
