#ifndef _SOCKET_STATIC_H_
#define _SOCKET_STATIC_H_
#ifdef __cplusplus
extern "C" {
#endif


//teddy 240122
/**
    @ingroup WIZnet_socket_APIs
    @brief Try to connect to a <b>TCP SERVER</b>.
    @details It sends a connection-reqeust message to the server with destination IP address and port number passed as parameter.\n
            SOCKET <i>sn</i> is used as active(<b>TCP CLIENT</b>) mode.
    @param sn SOCKET number. It should be <b>0 ~ @ref _WIZCHIP_SOCK_NUM_</b>.
    @param addr Pointer variable of destination IPv6 or IPv4 address.
    @param port Destination port number.
    @param addrlen the length of <i>addr</i>. \n <- removed
                  If addr is IPv6 address it should be 16,else if addr is IPv4 address it should be 4. Otherwize, return @ref SOCKERR_IPINVALID.
    @return Success : @ref SOCK_OK \n
           Fail    :\n @ref SOCKERR_SOCKNUM   - Invalid socket number\n
                       @ref SOCKERR_SOCKMODE  - Invalid socket mode\n
                       @ref SOCKERR_SOCKINIT  - Socket is not initialized\n
                       @ref SOCKERR_IPINVALID - Wrong server IP address\n
                       @ref SOCKERR_PORTZERO  - Server port zero\n
                       @ref SOCKERR_TIMEOUT   - Timeout occurred during request connection\n
                       @ref SOCK_BUSY         - In non-block io mode, it returns immediately\n
    @note It is valid only in TCP client mode. \n
         In block io mode, it does not return until connection is completed. \n
         In Non-block io mode(@ref SF_IO_NONBLOCK), it returns @ref SOCK_BUSY immediately.
*/
static int8_t connect_IO_6(uint8_t sn, uint8_t * addr, uint16_t port, uint8_t addrlen);
//int8_t connect(uint8_t sn, uint8_t * addr, uint16_t port, uint8_t addrlen);

/**
    @ingroup WIZnet_socket_APIs
    @brief Send datagram to the peer specifed by destination IP address and port number passed as parameter.
    @details It sends datagram data by using UDP,IPRAW, or MACRAW mode SOCKET.
    @param sn SOCKET number. It should be <b>0 ~ @ref _WIZCHIP_SOCK_NUM_</b>.
    @param buf Pointer of data buffer to be sent.
    @param len The byte length of data in buf.
    @param addr Pointer variable of destination IPv6 or IPv4 address.
    @param port Destination port number.
    @param addrlen the length of <i>addr</i>. \n
                  If addr is IPv6 address it should be 16,else if addr is IPv4 address it should be 4. Otherwize, return @ref SOCKERR_IPINVALID.
    @return Success : The real sent data size. It may be equal to <i>len</i> or small.\n
           Fail    :\n @ref SOCKERR_SOCKNUM     - Invalid SOCKET number \n
                       @ref SOCKERR_SOCKMODE    - Invalid operation in the SOCKET \n
                       @ref SOCKERR_SOCKSTATUS  - Invalid SOCKET status for SOCKET operation \n
                       @ref SOCKERR_IPINVALID   - Invalid IP address\n
                       @ref SOCKERR_PORTZERO    - Destination port number is zero\n
                       @ref SOCKERR_DATALEN     - Invalid data length \n
                       @ref SOCKERR_SOCKCLOSED  - SOCKET unexpectedly closed \n
                       @ref SOCKERR_TIMEOUT     - Timeout occurred \n
                       @ref SOCK_BUSY           - SOCKET is busy.
    @note It is valid only in @ref Sn_MR_UDP4, @ref Sn_MR_UDP6, @ref Sn_MR_UDPD, @ref Sn_MR_IPRAW4, @ref Sn_MR_IPRAW6, and @ref Sn_MR_MACRAW. \n
         In UDP mode, It can send data as many as SOCKET RX buffer size if data is greater than SOCKET TX buffer size. \n
         In IPRAW and MACRAW mode, It should send data as many as MTU(maxium transmission unit) if data is greater than MTU. That is, <i>len</i> can't exceed to MTU.
         In block io mode, It doesn't return until data send is completed.
         In non-block io mode(@ref SF_IO_NONBLOCK), It return @ref SOCK_BUSY immediately when SOCKET transimttable buffer size is not enough.
*/
//int32_t sendto(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t port, uint8_t addrlen);
static int32_t sendto_IO_6(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t port, uint8_t addrlen);

static int32_t recvfrom_IO_6(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t *port, uint8_t *addrlen);


#ifdef __cplusplus
}
#endif

#endif   // _SOCKET_H_