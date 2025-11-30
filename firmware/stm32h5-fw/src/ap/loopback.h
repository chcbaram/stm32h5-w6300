#ifndef _LOOPBACK_H_
#define _LOOPBACK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ap_def.h"

/* Loopback test debug message printout enable */
#if 0
#define	_LOOPBACK_DEBUG_
#endif


#define SOCK_TCP4			 (Sn_MR_TCP)
#define SOCK_TCP6			 (Sn_MR_TCP6)
#define SOCK_TCPD			 (Sn_MR_TCPD)

#define SOCK_UDP4			 (Sn_MR_UDP4)
#define SOCK_UDP6			 (Sn_MR_UDP6)
#define SOCK_UDPD			 (Sn_MR_UDPD)

#define AS_IPV4        2
#define AS_IPV6        23
#define AS_IPDUAL      11


/* DATA_BUF_SIZE define for Loopback example */
#ifndef DATA_BUF_SIZE
	#define DATA_BUF_SIZE			(8 * 1024)
#endif

/************************/
/* Select LOOPBACK_MODE */
/************************/
#define LOOPBACK_MAIN_NOBLOCK    0
#define LOOPBACK_MODE   LOOPBACK_MAIN_NOBLOCK

#if ((_WIZCHIP_ == 6100) || (_WIZCHIP_ == 6300))
int8_t set_loopback_mode_W6x00 (uint8_t get_loopback_mode ) ;
int8_t check_loopback_mode_W6x00();
#endif 

/* TCP server Loopback test example */
int32_t loopback_tcps(uint8_t sn, uint8_t* buf, uint16_t port);

/* TCP client Loopback test example */
int32_t loopback_tcpc(uint8_t sn, uint8_t* buf, uint8_t* destip, uint16_t destport);

/* UDP Loopback test example */
int32_t loopback_udps(uint8_t sn, uint8_t* buf, uint16_t port);

/* UDP Client Loopback test example */
int32_t loopback_udpc(uint8_t sn, uint8_t* buf, uint8_t* destip, uint16_t destport);

//teddy 240122

#ifdef __cplusplus
}
#endif

#endif
