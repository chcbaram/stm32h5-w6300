#include "usb.h"

#ifdef _USE_HW_USB
#if CFG_TUD_HID


//-- WebHID 채널. 실제 명령 처리는 ap/modules/cmd/driver/cmd_hid.c 가 한다.
//
#ifdef _USE_HW_CMD
extern void cmdHidRxReport(uint8_t const *buffer, uint16_t bufsize);
#endif


uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
  (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
  (void)instance; (void)report_id; (void)report_type;

#ifdef _USE_HW_CMD
  // OUT 엔드포인트로 온 리포트. USB 콜백이므로 링버퍼에 넣기만 한다.
  cmdHidRxReport(buffer, bufsize);
#else
  (void)buffer; (void)bufsize;
#endif
}

#endif
#endif
