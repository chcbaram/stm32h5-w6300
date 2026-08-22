#ifndef USB_H_
#define USB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

#ifdef _USE_HW_USB

#include "tusb.h"


typedef enum UsbMode
{
  USB_NON_MODE,
  USB_CDC_MODE,
  USB_MSC_MODE,
} UsbMode_t;

typedef enum UsbType
{
  USB_CON_CDC = 0,
  USB_CON_CLI = 1,
} UsbType_t;


bool      usbInit(void);
void      usbDeInit(void);
bool      usbIsInit(void);
bool      usbUpdate(void);
bool      usbIsOpen(void);
bool      usbIsConnect(void);
void      usbConnect(void);
void      usbDisconnect(void);

UsbMode_t usbGetMode(void);
UsbType_t usbGetType(void);

size_t    usbGetSerial(uint16_t desc_str1[], size_t max_chars);

#endif

#ifdef __cplusplus
}
#endif

#endif
