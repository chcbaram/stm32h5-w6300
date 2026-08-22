#include "cmd_task.h"

#if defined(_USE_HW_CMD) && defined(_USE_HW_USB) && (HW_USE_HID == 1)

#include "hid.h"
#include "qbuffer.h"


//-- HID 채널 드라이버.
//
//   HID 는 스트림이 아니라 64바이트 고정 리포트 단위다. cmd.c 는 바이트 스트림을
//   기대하므로, 수신 리포트를 링버퍼에 풀어 넣고 송신은 64바이트로 잘라 보낸다.
//
//   리포트 [0]  = 유효 바이트 수 (1~63)
//   리포트 [1:] = 페이로드
//
//   길이를 앞에 두는 이유: HID 는 항상 64바이트를 꽉 채워 보내므로 패딩과 실제
//   데이터를 구분할 방법이 필요하다. 패딩을 그냥 흘려보내면 cmd.c 의 패킷 파서가
//   쓰레기 바이트를 먹는다.
//
//   부트로더(TinyUSB)의 같은 파일과 리포트 규약이 동일해야 한다. 호스트(웹/툴)는
//   부트로더와 앱을 구분하지 않고 같은 코드로 말을 건다.
//
#define HID_RPT_SIZE      HID_PACKET_SIZE           // 64
#define HID_PAYLOAD_MAX   (HID_RPT_SIZE - 1)        // 63
#define HID_RX_BUF_SIZE   2048

static uint8_t   rx_buf[HID_RX_BUF_SIZE];
static qbuffer_t rx_q;
static bool      is_init = false;


static void drvHidInitOnce(void)
{
  if (is_init)
    return;
  qbufferCreate(&rx_q, rx_buf, HID_RX_BUF_SIZE);
  is_init = true;
}

//-- USB 스택이 호스트로부터 리포트를 받으면 부른다(hid.c 의 fops->Receive).
//   USB 인터럽트 문맥이므로 링버퍼에 넣기만 하고 처리는 메인 루프에서 한다.
//
void drvHidRxReport(uint8_t const *p_buf, uint16_t length)
{
  uint8_t n;

  drvHidInitOnce();

  if (length < 1)
    return;

  n = p_buf[0];
  if (n > HID_PAYLOAD_MAX)
    n = HID_PAYLOAD_MAX;
  if (n > length - 1)
    n = (uint8_t)(length - 1);

  qbufferWrite(&rx_q, (uint8_t *)&p_buf[1], n);
}


static bool drvHidOpen(void *args)
{
  (void)args;
  drvHidInitOnce();
  return true;
}

static bool drvHidClose(void *args)
{
  (void)args;
  return true;
}

static uint32_t drvHidAvailable(void *args)
{
  (void)args;
  drvHidInitOnce();
  return qbufferAvailable(&rx_q);
}

static bool drvHidFlush(void *args)
{
  (void)args;
  drvHidInitOnce();
  qbufferFlush(&rx_q);
  return true;
}

static uint8_t drvHidRead(void *args)
{
  uint8_t data = 0;

  (void)args;
  drvHidInitOnce();
  qbufferRead(&rx_q, &data, 1);
  return data;
}

static uint32_t drvHidWrite(void *args, uint8_t *p_data, uint32_t length)
{
  uint32_t sent = 0;

  (void)args;

  if (hidIsConnect() != true)
    return 0;

  while (sent < length)
  {
    uint8_t  rpt[HID_RPT_SIZE];
    uint32_t n = length - sent;
    uint32_t pre_time;

    if (n > HID_PAYLOAD_MAX)
      n = HID_PAYLOAD_MAX;

    memset(rpt, 0, sizeof(rpt));
    rpt[0] = (uint8_t)n;
    memcpy(&rpt[1], &p_data[sent], n);

    // IN 엔드포인트가 빌 때까지 기다린다. 완료는 USB 인터럽트가 알려주므로
    // 여기서는 폴링만 한다.
    pre_time = millis();
    while (hidIsReady() != true)
    {
      if (millis() - pre_time >= 100)
        return sent;
    }

    if (hidWrite(rpt, sizeof(rpt)) == 0)
      return sent;

    sent += n;
  }
  return sent;
}


cmd_driver_t drv_hid_driver =
{
  .open      = drvHidOpen,
  .close     = drvHidClose,
  .available = drvHidAvailable,
  .flush     = drvHidFlush,
  .read      = drvHidRead,
  .write     = drvHidWrite,
};

#endif
