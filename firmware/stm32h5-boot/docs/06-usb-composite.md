# 06. USB composite (TinyUSB MSC + CDC + HID)

## 목적

하나의 장치가 세 가지 업데이트 경로를 동시에 제공한다.
MSC(UF2 드래그&드롭), CDC(CLI + cmd 패킷), HID(WebHID).

## 대상 파일

- `src/hw/driver/usb/{usb.c, usb.h, usb_desc.c, tusb_config.h, usb_hid.c}`
- `src/hw/driver/cdc.c` (TinyUSB 용으로 재작성)
- `src/lib/tinyusb/**` (0.18.0)

## 인터페이스 / 엔드포인트

| 인터페이스 | 엔드포인트 | 용도 |
|---|---|---|
| ITF0 CDC Comm | `0x81` notif (16B) | |
| ITF1 CDC Data | `0x02` OUT / `0x82` IN (64B) | CLI + cmd 패킷 |
| ITF2 MSC | `0x03` OUT / `0x83` IN (64B) | UF2 드래그&드롭 |
| ITF3 HID | `0x04` OUT / `0x84` IN (64B, bInterval=1) | WebHID |

PMA 사용량 = BTABLE 64 + EP0 128 + notif 16 + CDC 128 + MSC 128 + HID 128 = **592B**
(USB_DRD_FS PMA 2KB). 엔드포인트 채널은 8개 중 5개 사용.

CDC 가 IAD 를 쓰므로 디바이스 클래스는 `TUSB_CLASS_MISC` / `MISC_SUBCLASS_COMMON` /
`MISC_PROTOCOL_IAD` 여야 한다.

HID 리포트 디스크립터는 `TUD_HID_REPORT_DESC_GENERIC_INOUT` 을 쓴다.
**vendor-defined usage page(0xFF00)** 라서 OS 가 키보드/마우스로 오인하지 않고,
WebHID 의 장치 선택창에 잡힌다.

## USB 초기화

```c
PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

// PA11(DM)/PA12(DP). H5 의 USB_DRD_FS 는 전용 아날로그 핀이라 AF 설정이 없다.
GPIO_InitStruct.Pin  = GPIO_PIN_11 | GPIO_PIN_12;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

__HAL_RCC_USB_CLK_ENABLE();
HAL_PWREx_EnableVddUSB();

HAL_NVIC_SetPriority(USB_DRD_FS_IRQn, 5, 0);   // 레퍼런스에 누락된 부분
HAL_NVIC_EnableIRQ(USB_DRD_FS_IRQn);

tusb_init(BOARD_TUD_RHPORT, &dev_init);
```

`bsp.c` 의 `SystemClock_Config()` 가 이미 HSI48 을 켠다. CRS 는 쓰지 않는다
(`stm32h5-fw` 도 동일).

> 레퍼런스(`stm32h562rg-tinyusb/usb.c`)에는 NVIC 설정이 없다. TinyUSB 의
> `dcd_init()` 이 `dcd_int_enable()` 로 IRQ 를 켜지만 우선순위가 기본값(0, 최고)이
> 되므로 명시적으로 5 로 낮췄다.

## usbInit() 호출 위치

`hwInit()` 이 아니라 `apInit()` 에서 `bootUp()` **뒤에** 호출한다.

```c
void apInit(void)  { bootUp(); usbInit(); bootInit(); uf2Init(); moduleInit(); }
```

앱으로 점프하는 경우 USB 열거를 아예 시작하지 않는다. 호스트에 장치가 나타났다
곧바로 사라지는 것을 막는다.

## 함정 / 주의사항

### `tud_task()` 를 부르지 않으면 열거되지 않는다

`usbInit()` 이 성공해도 호스트에 아무것도 보이지 않는 상태를 처음에 겪었다.
원인은 메인 루프에서 `usbUpdate()`(= `tud_task()`)를 호출하지 않은 것이었다.

```c
void update(void const *arg)      // ap 모듈
{
  usbUpdate();
  uf2Update();
  updateLed();
}
```

`cliLoopIdle()` 도 `moduleUpdate()` 를 부르므로 `delay()` 블로킹 구간에서도
USB 가 계속 돈다.

### `bsp.c` 가 `usbDeInit()` 을 부른다

`bspDeInit()` 안에서 호출하므로 `bsp.c` 에 `#ifdef _USE_HW_USB #include "usb.h" #endif`
가 필요하다.

## 검증

부트 모드로 진입한 뒤 호스트에서 확인한다.

## 실측 결과

```
[OK] usbInit()
     MSC 1 / CDC 1 / HID 1
```

호스트(macOS):

| 항목 | 값 |
|---|---|
| Product | `STM32H5-W6300-BOOT` |
| Serial | `460046000751333134383538` (STM32 96bit UID) |
| CDC | `/dev/cu.usbmodem1412301` 신규 포트 |
| HID | VID `0xCAFE` PID `0xB003`, **usage_page `0xFF00`** |
| MSC | `/dev/disk4`, 볼륨 `H5BOOT` 16.8 MB |

USB CDC 로 CLI 접속도 정상이다.

```
cli# usb info
init      : 1
mounted   : 1
connected : 1
suspended : 0
cdc conn  : 1
```

빌드 크기는 TinyUSB 3종 클래스 추가로 `56,768 -> 71,024 B` (약 14KB 증가),
126KB 중 55.0%.
