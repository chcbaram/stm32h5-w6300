#include "usb.h"



#ifdef _USE_HW_USB
#include "cdc.h"
#include "cli.h"
#ifdef USE_USBD_COMPOSITE
#include "usbd_composite_builder.h"
#endif

static bool is_init = false;
static UsbMode_t is_usb_mode = USB_NON_MODE;

USBD_HandleTypeDef USBD_Device;
extern PCD_HandleTypeDef hpcd_USB_DRD_FS;


extern USBD_DescriptorsTypeDef MSC_Desc;
#if HW_USE_MSC == 1
extern USBD_StorageTypeDef USBD_DISK_fops;
#endif

#if HW_USE_HID == 1
extern USBD_HID_ItfTypeDef USBD_HID_fops;
#endif

#if HW_USE_CMP == 1
//   composite 빌더에 넘길 엔드포인트 표.
//   HID: [0]=IN, [1]=OUT   CDC: [0]=IN, [1]=OUT, [2]=notification
//   순서가 usbd_composite_builder.c 의 AssignEp() 호출 순서와 일대일 대응한다.
static uint8_t hid_ep_tbl[] = {HID_EPIN_ADDR, HID_EPOUT_ADDR};
static uint8_t cdc_ep_tbl[] = {CDC_IN_EP, CDC_OUT_EP, CDC_CMD_EP};
#endif

static uint8_t cdc_class_id = 0;
static uint8_t hid_class_id = 0;

#if CLI_USE(HW_USB)
static void cliCmd(cli_args_t *args);
#endif




bool usbInit(void)
{
#if CLI_USE(HW_USB)
  cliAdd("usb", cliCmd);
#endif
  return true;
}

bool usbBegin(UsbMode_t usb_mode)
{
  is_init = true;

  //   USBD_free() 가 실제로 반환하지 않는 bump 할당기라 여기서 되감아야
  //   usbDeInit() 후 재시작이 가능하다.
  USBD_static_reset();
#ifdef USE_USBD_COMPOSITE
  USBD_CMPSIT_Reset();
#endif

  if (usb_mode == USB_CDC_MODE)
  {
    /* Init Device Library */
    USBD_Init(&USBD_Device, &VCP_Desc, DEVICE_HS);

    /* Add Supported Class */
    USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS);

    /* Add CDC Interface Class */
    USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);

    /* Start Device Process */
    USBD_Start(&USBD_Device);

    HAL_PWREx_EnableUSBVoltageDetector();

    is_usb_mode = USB_CDC_MODE;
    
    logPrintf("[OK] usbBegin()\n");
    logPrintf("     USB_CDC\r\n");
  }
  else if (usb_mode == USB_MSC_MODE)
  {
    #if HW_USE_MSC == 1
    /* Init Device Library */
    USBD_Init(&USBD_Device, &MSC_Desc, DEVICE_HS);

    /* Add Supported Class */
    USBD_RegisterClass(&USBD_Device, USBD_MSC_CLASS);

    /* Add Storage callbacks for MSC Class */
    USBD_MSC_RegisterStorage(&USBD_Device, &USBD_DISK_fops);

    /* Start Device Process */
    USBD_Start(&USBD_Device);

    HAL_PWREx_EnableUSBVoltageDetector();

    is_usb_mode = USB_MSC_MODE;

    logPrintf("[OK] usbBegin()\n");
    logPrintf("     USB_MSC\r\n");
    #endif
  }
  else if (usb_mode == USB_CMP_MODE)
  {
    #if HW_USE_CMP == 1
    USBD_Init(&USBD_Device, &CMP_Desc, DEVICE_HS);

    //-- 등록 순서가 인터페이스 번호 순서다.
    //   CDC 를 먼저 등록해 ITF0/ITF1 을 차지하게 한다. 윈도우는 IAD 로 묶인
    //   CDC 가 인터페이스 0 에서 시작할 때 가장 얌전하게 붙는다.
    //
    cdc_class_id = (uint8_t)USBD_Device.classId;
    USBD_RegisterClassComposite(&USBD_Device, USBD_CDC_CLASS, CLASS_TYPE_CDC, cdc_ep_tbl);

    #if HW_USE_HID == 1
    hid_class_id = (uint8_t)USBD_Device.classId;
    USBD_RegisterClassComposite(&USBD_Device, USBD_HID_CLASS, CLASS_TYPE_HID, hid_ep_tbl);
    #endif

    //-- fops 등록은 반드시 classId 를 되돌려 놓고 해야 한다.
    //
    //   USBD_xxx_RegisterInterface() 는 pdev->pUserData[pdev->classId] 에 쓰는데,
    //   USBD_RegisterClassComposite() 가 이미 classId 를 증가시킨 뒤다. 등록
    //   직후에 부르면 다음 클래스의 슬롯(또는 배열 밖)에 저장되고, 정작 자기
    //   클래스의 Init() 은 pUserData 가 NULL 인 채로 돈다. ST 예제 코드도 이
    //   순서로 되어 있어 그대로 따라 쓰면 CDC 수신 콜백이 조용히 죽는다.
    //
    USBD_Device.classId = cdc_class_id;
    USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);

    #if HW_USE_HID == 1
    USBD_Device.classId = hid_class_id;
    USBD_HID_RegisterInterface(&USBD_Device, &USBD_HID_fops);
    #endif

    USBD_Device.classId = 0;

    USBD_Start(&USBD_Device);

    HAL_PWREx_EnableUSBVoltageDetector();

    is_usb_mode = USB_CMP_MODE;

    logPrintf("[OK] usbBegin()\n");
    logPrintf("     USB_CMP (CDC + HID)\n");
    #endif
  }
  else
  {
    is_init = false;

    logPrintf("[NG] usbBegin()\n");
  }

  return is_init;
}

uint8_t usbGetCdcClassId(void)
{
  return cdc_class_id;
}

uint8_t usbGetHidClassId(void)
{
  return hid_class_id;
}

void usbDeInit(void)
{
  if (is_init == true)
  {
    USBD_DeInit(&USBD_Device);
  }
}

bool usbIsOpen(void)
{
  return cdcIsConnect();
}

bool usbIsConnect(void)
{
  if (USBD_Device.pClassData == NULL)
  {
    return false;
  }
  if (USBD_Device.dev_state != USBD_STATE_CONFIGURED)
  {
    return false;
  }
  if (USBD_Device.dev_config == 0)
  {
    return false;
  }
  if (USBD_is_connected() == false)
  {
    return false;
  }
  
  return true;
}

UsbMode_t usbGetMode(void)
{
  return is_usb_mode;
}

UsbType_t usbGetType(void)
{
  return (UsbType_t)cdcGetType();
}

void USB_DRD_FS_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_DRD_FS);
}


#if CLI_USE(HW_USB)
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    while(cliKeepLoop())
    {
      cliPrintf("USB Mode    : %d\n", usbGetMode());
      cliPrintf("USB Connect : %d\n", usbIsConnect());
      cliPrintf("USB Open    : %d\n", usbIsOpen());
      // 호스트가 연 보율이 CDC 스트림의 주인을 정한다. 115200 이면 CLI, 그 외는 cmd.
      cliPrintf("CDC Baud    : %-8d\n", (int)cdcGetBaud());
      cliPrintf("CDC Owner   : %-4s\n", usbGetType() == USB_CON_CLI ? "CLI" : "CMD");
      cliPrintf("\x1B[%dA", 5);
      delay(100);
    }
    cliPrintf("\x1B[%dB", 5);

    ret = true;
  }

  //-- 열거 문제를 눈으로 확인하는 용도.
  //   인터페이스 번호가 겹치거나 엔드포인트 주소가 어긋나면 여기서 바로 보인다.
  //
  if (args->argc == 1 && args->isStr(0, "desc") == true)
  {
    uint8_t *p_desc = USBD_Device.pConfDesc;
    uint16_t len;

    if (p_desc == NULL)
    {
      cliPrintf("pConfDesc is NULL\n");
      return;
    }

    len = (uint16_t)(p_desc[2] | (p_desc[3] << 8));
    cliPrintf("wTotalLength   : %d\n", len);
    cliPrintf("bNumInterfaces : %d\n", p_desc[4]);
    cliPrintf("NumClasses     : %d\n", (int)USBD_Device.NumClasses);
    cliPrintf("cdc class id   : %d\n", cdc_class_id);
    cliPrintf("hid class id   : %d\n", hid_class_id);
    cliPrintf("\n");

    for (int i=0; i<len; i++)
    {
      if (i%16 == 0) cliPrintf("%03d : ", i);
      cliPrintf("%02X ", p_desc[i]);
      if (i%16 == 15) cliPrintf("\n");
    }
    if (len%16 != 0) cliPrintf("\n");

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "tx") == true)
  {
    uint32_t pre_time;
    uint32_t tx_cnt = 0;
    uint32_t sent_len = 0;

    pre_time = millis();
    while(cliKeepLoop())
    {
      if (millis()-pre_time >= 1000)
      {
        pre_time = millis();
        logPrintf("tx : %d KB/s\n", tx_cnt/1024);
        tx_cnt = 0;
      }
      sent_len = cdcWrite((uint8_t *)"123456789012345678901234567890\n", 31);
      tx_cnt += sent_len;
    }
    cliPrintf("\x1B[%dB", 2);

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "rx") == true)
  {
    uint32_t pre_time;
    uint32_t rx_cnt = 0;
    uint32_t rx_len;

    pre_time = millis();
    while(cliKeepLoop())
    {
      if (millis()-pre_time >= 1000)
      {
        pre_time = millis();
        logPrintf("rx : %d KB/s\n", rx_cnt/1024);
        rx_cnt = 0;
      }

      rx_len = cdcAvailable();

      for (int i=0; i<rx_len; i++)
      {
        cdcRead();
      }

      rx_cnt += rx_len;
    }
    cliPrintf("\x1B[%dB", 2);

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("usb info\n");
    cliPrintf("usb desc\n");
    cliPrintf("usb tx\n");
    cliPrintf("usb rx\n");
  }
}
#endif

#endif