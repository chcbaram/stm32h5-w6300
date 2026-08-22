/*
 * net_disc.h
 *
 *   LAN 안의 보드 찾기. UDP 브로드캐스트 비컨.
 */

#ifndef NET_DISC_H_
#define NET_DISC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ap_def.h"

#ifdef _USE_HW_WIZNET


#define NET_DISC_PORT         5300
#define NET_DISC_MAGIC_REQ    0x51445242UL      // "BRDQ" (LE)
#define NET_DISC_MAGIC_RSP    0x52445242UL      // "BRDR" (LE)
#define NET_DISC_VER          1

#define NET_DISC_NAME_LEN     24
#define NET_DISC_VER_LEN      16
#define NET_DISC_LIST_MAX     8

//-- 비컨. 질의와 응답이 같은 구조를 쓴다(매직만 다르다).
//
//   질의는 ip/mac/name 을 채우지 않아도 되지만, 채워 보내면 받는 쪽이 누가
//   물었는지 알 수 있어 그대로 채운다.
//
typedef struct
{
  uint32_t magic;
  uint8_t  ver;
  uint8_t  mode;                        // HW_DEV_MODE_BOOT / _APP
  uint8_t  rsv[2];
  uint8_t  ip[4];
  uint8_t  mac[6];
  uint8_t  rsv2[2];
  char     name[NET_DISC_NAME_LEN];
  char     version[NET_DISC_VER_LEN];
} __attribute__((packed)) net_beacon_t;   // 60B


bool    netDiscInit(void);
void    netDiscUpdate(void);

//-- LAN 을 훑어 보드 목록을 채운다.
//
//   질의를 브로드캐스트하고 timeout_ms 동안 응답을 모은다. 이 보드 자신은
//   항상 첫 항목이다 - 칩이 자기 브로드캐스트를 되받는지는 보장되지 않는다.
//   찾은 개수를 돌려준다.
//
uint8_t netDiscScan(net_beacon_t *p_list, uint8_t list_max, uint32_t timeout_ms);


#endif

#ifdef __cplusplus
}
#endif

#endif
