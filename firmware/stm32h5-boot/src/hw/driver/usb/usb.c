#include "usb.h"


#ifdef _USE_HW_USB
#include "cli.h"
#ifdef _USE_HW_CDC
#include "cdc.h"
#endif


static bool is_init = false;

#if CLI_USE(HW_USB)
static void cliUsb(cli_args_t *args);
#endif



bool usbInit(void)
{
  tusb_rhport_init_t dev_init =
  {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  GPIO_InitTypeDef GPIO_InitStruct = {0};


  // USB 클럭은 HSI48. bsp 의 SystemClock_Config() 에서 HSI48 을 이미 켠다.
  //
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInitStruct.UsbClockSelection    = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    logPrintf("[E_] usbInit() clk\n");
    return false;
  }

  // PA11(DM) / PA12(DP). H5 의 USB_DRD_FS 는 전용 아날로그 핀이라 AF 설정이 없다.
  //
  GPIO_InitStruct.Pin   = (GPIO_PIN_11 | GPIO_PIN_12);
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  __HAL_RCC_USB_CLK_ENABLE();

#if defined (PWR_USBSCR_USB33DEN)
  HAL_PWREx_EnableVddUSB();
#endif

  // TinyUSB 의 dcd_init() 이 dcd_int_enable() 로 IRQ 를 켜지만 우선순위는
  // 기본값(0, 최고)이 된다. 명시적으로 낮춰둔다.
  //
  HAL_NVIC_SetPriority(USB_DRD_FS_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USB_DRD_FS_IRQn);

  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  is_init = true;
  logPrintf("[OK] usbInit()\n");
  logPrintf("     MSC %d / CDC %d / HID %d\n", CFG_TUD_MSC, CFG_TUD_CDC, CFG_TUD_HID);

#if CLI_USE(HW_USB)
  cliAdd("usb", cliUsb);
#endif
  return true;
}

void usbDeInit(void)
{
  if (is_init != true)
    return;

  tud_disconnect();
  HAL_NVIC_DisableIRQ(USB_DRD_FS_IRQn);
  __HAL_RCC_USB_CLK_DISABLE();
  is_init = false;
}

bool usbIsInit(void)
{
  return is_init;
}

bool usbUpdate(void)
{
  if (is_init != true)
    return false;

  tud_task();
  return true;
}

bool usbIsConnect(void)
{
  if (is_init != true)
    return false;

  return (tud_connected() && !tud_suspended());
}

bool usbIsOpen(void)
{
#ifdef _USE_HW_CDC
  return cdcIsConnect();
#else
  return usbIsConnect();
#endif
}

void usbConnect(void)
{
  if (is_init)
    tud_connect();
}

void usbDisconnect(void)
{
  if (is_init)
    tud_disconnect();
}

UsbMode_t usbGetMode(void)
{
  return is_init ? USB_MSC_MODE : USB_NON_MODE;
}

//   호스트가 CDC 를 연 보율로 갈린다. cdc.c 의 cdcGetType() 을 본다.
UsbType_t usbGetType(void)
{
  return (UsbType_t)cdcGetType();
}

//-- STM32 96bit UID 로 시리얼 문자열을 만든다.
//
size_t usbGetSerial(uint16_t desc_str1[], size_t max_chars)
{
  uint8_t uid[12] TU_ATTR_ALIGNED(4);
  volatile uint32_t *stm32_uuid = (volatile uint32_t *)UID_BASE;
  uint32_t *id32 = (uint32_t *)(uintptr_t)uid;
  size_t uid_len = 12;

  id32[0] = stm32_uuid[0];
  id32[1] = stm32_uuid[1];
  id32[2] = stm32_uuid[2];

  if (uid_len > max_chars / 2)
    uid_len = max_chars / 2;

  for (size_t i = 0; i < uid_len; i++)
  {
    for (size_t j = 0; j < 2; j++)
    {
      const char nibble_to_hex[16] = {'0','1','2','3','4','5','6','7',
                                      '8','9','A','B','C','D','E','F'};
      uint8_t const nibble = (uid[i] >> (j * 4)) & 0xF;
      desc_str1[i * 2 + (1 - j)] = nibble_to_hex[nibble];   // UTF-16-LE
    }
  }
  return 2 * uid_len;
}

void USB_DRD_FS_IRQHandler(void)
{
  tud_int_handler(BOARD_TUD_RHPORT);
}


#if CLI_USE(HW_USB)
void cliUsb(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info"))
  {
    while (cliKeepLoop())
    {
      cliPrintf("init      : %d\n", is_init);
      cliPrintf("mounted   : %d\n", tud_mounted());
      cliPrintf("connected : %d\n", tud_connected());
      cliPrintf("suspended : %d\n", tud_suspended());
#if CFG_TUD_CDC
      cliPrintf("cdc conn  : %d\n", tud_cdc_n_connected(0));
      // 호스트가 연 보율이 CDC 스트림의 주인을 정한다. 115200 이면 CLI, 그 외는 cmd.
      cliPrintf("cdc baud  : %-8d\n", (int)cdcGetBaud());
      cliPrintf("cdc owner : %-4s\n", usbGetType() == USB_CON_CLI ? "CLI" : "CMD");
      cliMoveUp(7);
#else
      cliMoveUp(4);
#endif
      delay(100);
    }
#if CFG_TUD_CDC
    cliMoveDown(7);
#else
    cliMoveDown(4);
#endif
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("usb info\n");
  }
}
#endif

#endif
