#include "usb.h"

#ifdef _USE_HW_USB
#if CFG_TUD_HID


//-- WebHID 채널. 실제 명령 처리는 11단계의 cmd_hid.c 가 담당한다.
//   지금은 열거만 되도록 최소 콜백을 둔다.
//
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
  (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

#endif
#endif
