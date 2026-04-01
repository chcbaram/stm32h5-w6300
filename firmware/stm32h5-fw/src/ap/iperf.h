#ifndef IPERF_H_
#define IPERF_H_


#include "hw.h"



#define IPERF_BUF_MAX_SIZE    (16 * 1024)


int32_t iperf_tcps(uint8_t sn, uint8_t *buf, uint16_t port);

#endif