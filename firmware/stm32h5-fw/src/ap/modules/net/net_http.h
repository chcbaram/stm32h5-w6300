/*
 * net_http.h
 *
 *   보드 자체 웹서버. 최소 HTTP 만 한다.
 */

#ifndef NET_HTTP_H_
#define NET_HTTP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ap_def.h"

#ifdef _USE_HW_WIZNET


#define NET_HTTP_PORT     80

typedef struct
{
  const char    *url;
  const char    *mime;
  const uint8_t *data;      // gzip 된 내용
  uint32_t       size;
} web_file_t;

//   tools/web/gen_web.py 가 만든다.
extern const web_file_t web_file_tbl[];
extern const uint32_t   web_file_cnt;


bool netHttpInit(void);
void netHttpUpdate(void);


#endif

#ifdef __cplusplus
}
#endif

#endif
