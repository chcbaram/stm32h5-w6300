#include "cdc.h"

#ifdef _USE_HW_CDC
#include "usb.h"


static bool is_init = false;



bool cdcInit(void)
{
  is_init = true;
  return true;
}

bool cdcIsInit(void)
{
  return is_init;
}

bool cdcIsConnect(void)
{
  if (usbIsInit() != true)
    return false;

  return tud_cdc_n_connected(0);
}

uint32_t cdcAvailable(void)
{
  if (usbIsInit() != true)
    return 0;

  return tud_cdc_n_available(0);
}

uint8_t cdcRead(void)
{
  uint8_t buf[1] = {0};

  if (usbIsInit() != true)
    return 0;

  tud_cdc_n_read(0, buf, 1);
  return buf[0];
}

uint32_t cdcWrite(uint8_t *p_data, uint32_t length)
{
  uint32_t pre_time;
  uint32_t sent_len = 0;

  if (cdcIsConnect() != true)
    return 0;

  pre_time = millis();
  while (sent_len < length)
  {
    uint32_t tx_len;

    usbUpdate();      // 블로킹 전송 중에도 tud_task() 를 계속 돌린다

    tx_len = tud_cdc_n_write(0, &p_data[sent_len], length - sent_len);
    if (tx_len > 0)
    {
      sent_len += tx_len;
    }

    if (cdcIsConnect() != true)
      break;

    if (millis() - pre_time >= 100)
      break;
  }
  tud_cdc_n_write_flush(0);

  return sent_len;
}

uint32_t cdcGetBaud(void)
{
  cdc_line_coding_t coding;

  if (usbIsInit() != true)
    return 0;

  tud_cdc_n_get_line_coding(0, &coding);
  return coding.bit_rate;
}

//-- 호스트가 연 보율로 CDC 스트림의 주인을 가른다.
//
//   115200 = 터미널(CLI), 그 외 = 호스트 툴(cmd 패킷).
//   둘이 같은 스트림에서 각자 읽으면 서로 바이트를 훔쳐 양쪽 다 깨진다.
//   판정은 cli_mgr.c 와 cmd_task.c 가 같이 쓴다.
//
uint8_t cdcGetType(void)
{
  if (cdcGetBaud() == 115200)
    return USB_CON_CLI;

  return USB_CON_CDC;
}

//-- 1200bps touch : 호스트가 1200bps 로 열었다 닫으면 부트로더로 진입한다.
//   부트로더 자신에서는 의미가 없지만, 앱이 같은 파일을 쓸 때를 위해 남겨둔다.
//
void tud_cdc_line_state_cb(uint8_t instance, bool dtr, bool rts)
{
  (void)rts;

  if (!dtr && instance == 0)
  {
    if (cdcGetBaud() == 1200)
    {
      // 부트로더에서는 이미 부트 모드이므로 아무것도 하지 않는다.
    }
  }
}

#endif
