//*****************************************************************************
//
//! \file W6300.c
//! \brief W6300 HAL Implements file.
//! \version 1.0.0
//! \date 2019/01/01
//! \par  Revision history
//!       <2019/01/01> 1st Release
//! \author MidnightCow
//! \copyright
//!
//! Copyright (c)  2019, WIZnet Co., LTD.
//!
//! Permission is hereby granted, free of charge, to any person obtaining a copy
//! of this software and associated documentation files (the "Software"), to deal
//! in the Software without restriction, including without limitation the rights
//! to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//! copies of the Software, and to permit persons to whom the Software is
//! furnished to do so, subject to the following conditions:
//!
//! The above copyright notice and this permission notice shall be included in
//! all copies or substantial portions of the Software.
//!
//! THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//! IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//! FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//! AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//! LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//! OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//! SOFTWARE.
//!
//*****************************************************************************

#include "w6300.h"
#include "wiz_spi.h"


#if _WIZCHIP_ == 6300
////////////////////////////////////////////////////////////////////////////////////////


enum
{
  WIZ_ERR_WIZCHIP_WRITE,
  WIZ_ERR_WIZCHIP_READ,
};


#define _W6300_SPI_OP_    _WIZCHIP_SPI_VDM_OP_
#define _W6300_SPI_READ_  (0x00 << 5) ///< SPI interface Read operation in Control Phase
#define _W6300_SPI_WRITE_ (0x01 << 5) ///< SPI interface Write operation in Control Phase

static bool is_init = false;
static bool is_init_spi = false;
static bool is_ready = false;
static uint16_t err_code = 0;


bool w6300Init(void)
{
  bool ret;


  is_init_spi = wizspiInit();

  if (is_init_spi)
  {
    if (WIZCHIP_READ(_CIDR_) == 0x61)
    {
      is_ready = true;
    }
  }

  ret = is_ready;

  logPrintf("[%s] w6300Init()\n", ret ? "OK" : "E_");  
  if (is_ready)    
    logPrintf("     CIDR : 0x%02X\n", WIZCHIP_READ(_CIDR_));
  else
    logPrintf("     Chip Not Found\n");

  is_init = ret;

  return ret;
}

bool w6300IsReady(void)
{
  return is_ready;
}

//////////////////////////////////////////////////
void WIZCHIP_WRITE(uint32_t AddrSel, uint8_t wb)
{
  bool     ret;
  uint8_t  opcode = 0;
  uint16_t addr   = 0;

  opcode = (uint8_t)(AddrSel & 0x000000FF);
  addr   = (uint16_t)((AddrSel & 0x00ffff00) >> 8);
  ret    = wizspiWrite(opcode, addr, &wb, 1, 10);
  if (!ret)
  {
    err_code = WIZ_ERR_WIZCHIP_WRITE;
  }
}

uint8_t WIZCHIP_READ(uint32_t AddrSel)
{
  bool     ret;
  uint8_t  opcode = 0;
  uint16_t addr   = 0;
  uint8_t  buf[1];

  opcode = (uint8_t)(AddrSel & 0x000000FF);
  addr   = (uint16_t)((AddrSel & 0x00ffff00) >> 8);
  ret    = wizspiRead(opcode, addr, buf, 1, 10);
  if (!ret)
  {
    err_code = WIZ_ERR_WIZCHIP_READ;
  }
  return buf[0];
}

void WIZCHIP_WRITE_BUF(uint32_t AddrSel, uint8_t *pBuf, datasize_t len)
{
  bool     ret;
  uint8_t  opcode = 0;
  uint16_t addr   = 0;

  opcode = (uint8_t)(AddrSel & 0x000000FF);
  addr   = (uint16_t)((AddrSel & 0x00ffff00) >> 8);
  ret    = wizspiWrite(opcode, addr, pBuf, len, 50);
  if (!ret)
  {
    err_code = WIZ_ERR_WIZCHIP_WRITE;
  }
}

void WIZCHIP_READ_BUF(uint32_t AddrSel, uint8_t *pBuf, datasize_t len)
{
  bool     ret;
  uint8_t  opcode = 0;
  uint16_t addr   = 0;

  opcode = (uint8_t)(AddrSel & 0x000000FF);
  addr   = (uint16_t)((AddrSel & 0x00ffff00) >> 8);
  ret    = wizspiRead(opcode, addr, pBuf, len, 10);
  if (!ret)
  {
    err_code = WIZ_ERR_WIZCHIP_READ;
  }  
}

uint16_t getSn_TX_FSR(uint8_t sn)
{
  uint16_t prev_val = -1, val = 0;
  do
  {
    prev_val = val;
    val      = WIZCHIP_READ(_Sn_TX_FSR_(sn));
    val      = (val << 8) + WIZCHIP_READ(WIZCHIP_OFFSET_INC(_Sn_TX_FSR_(sn), 1));
  } while (val != prev_val);
  return val;
}

uint16_t getSn_RX_RSR(uint8_t sn)
{
  uint16_t prev_val = -1, val = 0;
  do
  {
    prev_val = val;
    val      = WIZCHIP_READ(_Sn_RX_RSR_(sn));
    val      = (val << 8) + WIZCHIP_READ(WIZCHIP_OFFSET_INC(_Sn_RX_RSR_(sn), 1));
  } while (val != prev_val);
  return val;
}

void wiz_send_data(uint8_t sn, uint8_t *wizdata, uint16_t len)
{
  uint16_t ptr     = 0;
  uint32_t addrsel = 0;
  ptr              = getSn_TX_WR(sn);
  addrsel          = ((uint32_t)ptr << 8) + WIZCHIP_TXBUF_BLOCK(sn);
  WIZCHIP_WRITE_BUF(addrsel, wizdata, len);
  ptr += len;
  setSn_TX_WR(sn, ptr);
}

void wiz_recv_data(uint8_t sn, uint8_t *wizdata, uint16_t len)
{
  uint16_t ptr     = 0;
  uint32_t addrsel = 0;
  if (len == 0)
  {
    return;
  }
  ptr     = getSn_RX_RD(sn);
  addrsel = ((uint32_t)ptr << 8) + WIZCHIP_RXBUF_BLOCK(sn);
  WIZCHIP_READ_BUF(addrsel, wizdata, len);
  ptr += len;
  setSn_RX_RD(sn, ptr);
}

void wiz_recv_ignore(uint8_t sn, uint16_t len)
{
  setSn_RX_RD(sn, getSn_RX_RD(sn) + len);
}

#if 1
// 20231019 taylor
void wiz_delay_ms(uint32_t milliseconds)
{
  uint32_t i;
  for (i = 0; i < milliseconds; i++)
  {
    // Write any values to clear the TCNTCLKR register
    setTCNTRCLR(0xff);

    // Wait until counter register value reaches 10.(10 = 1ms : TCNTR is 100us tick counter register)
    while (getTCNTR() < 0x0a)
    {
    }
  }
}
#endif

/// @cond DOXY_APPLY_CODE
#if (_PHY_IO_MODE_ == _PHY_IO_MODE_MII_)
/// @endcond
void wiz_mdio_write(uint8_t phyregaddr, uint16_t var)
{
  setPHYRAR(phyregaddr);
  setPHYDIR(var);
  setPHYACR(PHYACR_WRITE);
  while (getPHYACR())
    ; // wait for command complete
}

uint16_t wiz_mdio_read(uint8_t phyregaddr)
{
  setPHYRAR(phyregaddr);
  setPHYACR(PHYACR_READ);
  while (getPHYACR())
    ; // wait for command complete
  return getPHYDOR();
}

/// @cond DOXY_APPLY_CODE
#endif
/// @endcond

////////////////////////////////////////////////////////////////////////////////////////
#endif
