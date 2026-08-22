/*
 * usbd_hid.c
 *
 *   ST USB Device Library 용 vendor-defined HID 클래스.
 *
 *   ST 표준 usbd_hid.c 는 키보드 IN 전용이라 호스트에서 보드로 내려오는 경로가
 *   없다. 여기서는 IN/OUT 각각 64바이트 인터럽트 엔드포인트를 열어 cmd 패킷
 *   프로토콜을 양방향으로 태운다.
 */

#include "usbd_hid.h"
#include "usbd_ctlreq.h"


static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
#ifndef USE_USBD_COMPOSITE
static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length);
#endif /* USE_USBD_COMPOSITE */

static uint8_t HIDInEpAdd  = HID_EPIN_ADDR;
static uint8_t HIDOutEpAdd = HID_EPOUT_ADDR;


USBD_ClassTypeDef USBD_HID =
{
  USBD_HID_Init,
  USBD_HID_DeInit,
  USBD_HID_Setup,
  NULL,               /* EP0_TxSent */
  NULL,               /* EP0_RxReady */
  USBD_HID_DataIn,
  USBD_HID_DataOut,
  NULL,               /* SOF */
  NULL,
  NULL,
#ifdef USE_USBD_COMPOSITE
  NULL,
  NULL,
  NULL,
  NULL,
#else
  USBD_HID_GetHSCfgDesc,
  USBD_HID_GetFSCfgDesc,
  USBD_HID_GetOtherSpeedCfgDesc,
  USBD_HID_GetDeviceQualifierDesc,
#endif /* USE_USBD_COMPOSITE */
};


//-- 리포트 디스크립터.
//   부트로더 TinyUSB 의 TUD_HID_REPORT_DESC_GENERIC_INOUT(64) 전개 결과와
//   바이트 단위로 동일하다. 여기가 어긋나면 웹페이지의 usagePage 필터가
//   앱을 걸러내지 못한다.
//
__ALIGN_BEGIN static uint8_t USBD_HID_ReportDesc[HID_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x06, 0x00, 0xFF,       /* Usage Page (Vendor Defined 0xFF00) */
  0x09, 0x01,             /* Usage (0x01)                       */
  0xA1, 0x01,             /* Collection (Application)           */

  0x09, 0x02,             /*   Usage (0x02)                     */
  0x15, 0x00,             /*   Logical Minimum (0)              */
  0x26, 0xFF, 0x00,       /*   Logical Maximum (255)            */
  0x75, 0x08,             /*   Report Size (8)                  */
  0x95, HID_EP_SIZE,      /*   Report Count (64)                */
  0x81, 0x02,             /*   Input (Data,Var,Abs)             */

  0x09, 0x03,             /*   Usage (0x03)                     */
  0x15, 0x00,             /*   Logical Minimum (0)              */
  0x26, 0xFF, 0x00,       /*   Logical Maximum (255)            */
  0x75, 0x08,             /*   Report Size (8)                  */
  0x95, HID_EP_SIZE,      /*   Report Count (64)                */
  0x91, 0x02,             /*   Output (Data,Var,Abs)            */

  0xC0                    /* End Collection                     */
};

//-- HID 기능 디스크립터.
//   composite 모드에서는 구성 디스크립터 안에 빌더가 따로 넣지만,
//   호스트가 GET_DESCRIPTOR(HID) 를 인터페이스로 직접 물어볼 때 쓴다.
//
__ALIGN_BEGIN static uint8_t USBD_HID_Desc[USB_HID_DESC_SIZ] __ALIGN_END =
{
  0x09,                   /* bLength */
  HID_DESCRIPTOR_TYPE,    /* bDescriptorType */
  0x11, 0x01,             /* bcdHID: 1.11 */
  0x00,                   /* bCountryCode */
  0x01,                   /* bNumDescriptors */
  HID_REPORT_DESC,        /* bDescriptorType */
  HID_REPORT_DESC_SIZE, 0x00,
};

#ifndef USE_USBD_COMPOSITE
__ALIGN_BEGIN static uint8_t USBD_HID_CfgDesc[USB_HID_CONFIG_DESC_SIZ] __ALIGN_END =
{
  0x09,                               /* bLength */
  USB_DESC_TYPE_CONFIGURATION,        /* bDescriptorType */
  LOBYTE(USB_HID_CONFIG_DESC_SIZ),
  HIBYTE(USB_HID_CONFIG_DESC_SIZ),
  0x01,                               /* bNumInterfaces */
  0x01,                               /* bConfigurationValue */
  0x00,                               /* iConfiguration */
#if (USBD_SELF_POWERED == 1U)
  0xC0,
#else
  0x80,
#endif /* USBD_SELF_POWERED */
  USBD_MAX_POWER,

  /* Interface */
  0x09,                               /* bLength */
  USB_DESC_TYPE_INTERFACE,            /* bDescriptorType */
  0x00,                               /* bInterfaceNumber */
  0x00,                               /* bAlternateSetting */
  0x02,                               /* bNumEndpoints */
  0x03,                               /* bInterfaceClass: HID */
  0x00,                               /* bInterfaceSubClass: no boot */
  0x00,                               /* bInterfaceProtocol: none */
  0x00,                               /* iInterface */

  /* HID */
  0x09,
  HID_DESCRIPTOR_TYPE,
  0x11, 0x01,
  0x00,
  0x01,
  HID_REPORT_DESC,
  HID_REPORT_DESC_SIZE, 0x00,

  /* Endpoint IN */
  0x07,
  USB_DESC_TYPE_ENDPOINT,
  HID_EPIN_ADDR,
  0x03,                               /* bmAttributes: Interrupt */
  LOBYTE(HID_EP_SIZE), HIBYTE(HID_EP_SIZE),
  HID_FS_BINTERVAL,

  /* Endpoint OUT */
  0x07,
  USB_DESC_TYPE_ENDPOINT,
  HID_EPOUT_ADDR,
  0x03,
  LOBYTE(HID_EP_SIZE), HIBYTE(HID_EP_SIZE),
  HID_FS_BINTERVAL,
};

__ALIGN_BEGIN static uint8_t USBD_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00, 0x02,
  0x00, 0x00, 0x00,
  0x40,
  0x01,
  0x00,
};
#endif /* USE_USBD_COMPOSITE */


static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  USBD_HID_HandleTypeDef *hhid;

  UNUSED(cfgidx);

  hhid = (USBD_HID_HandleTypeDef *)USBD_malloc(sizeof(USBD_HID_HandleTypeDef));
  if (hhid == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  pdev->pClassDataCmsit[pdev->classId] = (void *)hhid;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

#ifdef USE_USBD_COMPOSITE
  HIDInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
  HIDOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  hhid->Protocol   = 0U;
  hhid->IdleState  = 0U;
  hhid->AltSetting = 0U;
  hhid->state      = USBD_HID_IDLE;

  (void)USBD_LL_OpenEP(pdev, HIDInEpAdd, USBD_EP_TYPE_INTR, HID_EP_SIZE);
  pdev->ep_in[HIDInEpAdd & 0xFU].is_used = 1U;
  pdev->ep_in[HIDInEpAdd & 0xFU].bInterval = HID_FS_BINTERVAL;

  (void)USBD_LL_OpenEP(pdev, HIDOutEpAdd, USBD_EP_TYPE_INTR, HID_EP_SIZE);
  pdev->ep_out[HIDOutEpAdd & 0xFU].is_used = 1U;
  pdev->ep_out[HIDOutEpAdd & 0xFU].bInterval = HID_FS_BINTERVAL;

  if (pdev->pUserData[pdev->classId] != NULL)
  {
    ((USBD_HID_ItfTypeDef *)pdev->pUserData[pdev->classId])->Init();
  }

  /* OUT 수신을 미리 걸어둔다. 이걸 빠뜨리면 호스트 출력이 영영 오지 않는다. */
  (void)USBD_LL_PrepareReceive(pdev, HIDOutEpAdd, hhid->RxBuffer, HID_EP_SIZE);

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

#ifdef USE_USBD_COMPOSITE
  HIDInEpAdd  = USBD_CoreGetEPAdd(pdev, USBD_EP_IN,  USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
  HIDOutEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_OUT, USBD_EP_TYPE_INTR, (uint8_t)pdev->classId);
#endif /* USE_USBD_COMPOSITE */

  (void)USBD_LL_CloseEP(pdev, HIDInEpAdd);
  pdev->ep_in[HIDInEpAdd & 0xFU].is_used = 0U;
  pdev->ep_in[HIDInEpAdd & 0xFU].bInterval = 0U;

  (void)USBD_LL_CloseEP(pdev, HIDOutEpAdd);
  pdev->ep_out[HIDOutEpAdd & 0xFU].is_used = 0U;
  pdev->ep_out[HIDOutEpAdd & 0xFU].bInterval = 0U;

  if (pdev->pUserData[pdev->classId] != NULL)
  {
    ((USBD_HID_ItfTypeDef *)pdev->pUserData[pdev->classId])->DeInit();
  }

  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    pdev->pClassData = NULL;
  }

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  USBD_StatusTypeDef ret = USBD_OK;
  uint16_t len;
  uint8_t *pbuf;
  uint16_t status_info = 0U;

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS:
      switch (req->bRequest)
      {
        case USBD_HID_REQ_SET_PROTOCOL:
          hhid->Protocol = (uint8_t)(req->wValue);
          break;

        case USBD_HID_REQ_GET_PROTOCOL:
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->Protocol, 1U);
          break;

        case USBD_HID_REQ_SET_IDLE:
          hhid->IdleState = (uint8_t)(req->wValue >> 8);
          break;

        case USBD_HID_REQ_GET_IDLE:
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->IdleState, 1U);
          break;

        //-- 컨트롤 전송으로 내려오는 출력 리포트.
        //   WebHID 의 sendReport() 는 보통 OUT 엔드포인트를 쓰지만, OS/드라이버에
        //   따라 SET_REPORT 로 오는 경우가 있어 양쪽을 다 받는다.
        case USBD_HID_REQ_SET_REPORT:
          len = MIN(req->wLength, HID_EP_SIZE);
          (void)USBD_CtlPrepareRx(pdev, hhid->RxBuffer, len);
          break;

        case USBD_HID_REQ_GET_REPORT:
          len = MIN(req->wLength, HID_EP_SIZE);
          (void)USBD_CtlSendData(pdev, hhid->RxBuffer, len);
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_DESCRIPTOR:
          if ((req->wValue >> 8) == HID_REPORT_DESC)
          {
            len  = MIN(HID_REPORT_DESC_SIZE, req->wLength);
            pbuf = USBD_HID_ReportDesc;
          }
          else if ((req->wValue >> 8) == HID_DESCRIPTOR_TYPE)
          {
            len  = MIN(USB_HID_DESC_SIZ, req->wLength);
            pbuf = USBD_HID_Desc;
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
          }
          (void)USBD_CtlSendData(pdev, pbuf, len);
          break;

        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->AltSetting, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            hhid->AltSetting = (uint8_t)(req->wValue);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      ret = USBD_FAIL;
      break;
  }

  return (uint8_t)ret;
}

static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  UNUSED(epnum);

  if (hhid != NULL)
  {
    hhid->state = USBD_HID_IDLE;
  }
  return (uint8_t)USBD_OK;
}

static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  uint32_t length;

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  length = USBD_LL_GetRxDataSize(pdev, epnum);

  if (pdev->pUserData[pdev->classId] != NULL)
  {
    ((USBD_HID_ItfTypeDef *)pdev->pUserData[pdev->classId])->Receive(hhid->RxBuffer, length);
  }

  /* 다음 리포트를 다시 걸어둔다. */
  (void)USBD_LL_PrepareReceive(pdev, HIDOutEpAdd, hhid->RxBuffer, HID_EP_SIZE);

  return (uint8_t)USBD_OK;
}

#ifndef USE_USBD_COMPOSITE
static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

static uint8_t *USBD_HID_GetHSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

static uint8_t *USBD_HID_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_DeviceQualifierDesc);
  return USBD_HID_DeviceQualifierDesc;
}
#endif /* USE_USBD_COMPOSITE */


uint8_t USBD_HID_RegisterInterface(USBD_HandleTypeDef *pdev, USBD_HID_ItfTypeDef *fops)
{
  if (fops == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  pdev->pUserData[pdev->classId] = fops;

  return (uint8_t)USBD_OK;
}

#ifdef USE_USBD_COMPOSITE
uint8_t USBD_HID_IsReady(USBD_HandleTypeDef *pdev, uint8_t ClassId)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[ClassId];
#else
uint8_t USBD_HID_IsReady(USBD_HandleTypeDef *pdev)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[0];
#endif /* USE_USBD_COMPOSITE */

  if (hhid == NULL)
  {
    return 0U;
  }
  if (pdev->dev_state != USBD_STATE_CONFIGURED)
  {
    return 0U;
  }
  return (hhid->state == USBD_HID_IDLE) ? 1U : 0U;
}

#ifdef USE_USBD_COMPOSITE
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len, uint8_t ClassId)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[ClassId];
#else
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len)
{
  USBD_HID_HandleTypeDef *hhid = (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[0];
#endif /* USE_USBD_COMPOSITE */

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

#ifdef USE_USBD_COMPOSITE
  HIDInEpAdd = USBD_CoreGetEPAdd(pdev, USBD_EP_IN, USBD_EP_TYPE_INTR, ClassId);
#endif /* USE_USBD_COMPOSITE */

  if (pdev->dev_state != USBD_STATE_CONFIGURED)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (hhid->state != USBD_HID_IDLE)
  {
    return (uint8_t)USBD_BUSY;
  }

  hhid->state = USBD_HID_BUSY;
  (void)USBD_LL_Transmit(pdev, HIDInEpAdd, report, len);

  return (uint8_t)USBD_OK;
}

uint32_t USBD_HID_GetPollingInterval(USBD_HandleTypeDef *pdev)
{
  UNUSED(pdev);
  return (uint32_t)HID_FS_BINTERVAL;
}
