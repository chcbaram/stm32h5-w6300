#ifndef HW_DEF_H_
#define HW_DEF_H_



#include "bsp.h"
#include "assert_def.h"


#define _DEF_FIRMWATRE_VERSION    "V260822R1"
#define _DEF_BOARD_NAME           "STM32H5-W6300-FW"



#define _USE_HW_ASSERT
#define _USE_HW_FAULT
#define _USE_HW_WIZSPI


#define _USE_HW_LED
#define      HW_LED_MAX_CH          1

#define _USE_HW_UART
#define      HW_UART_MAX_CH         4
#define      HW_UART_CH_SWD         _DEF_UART1
#define      HW_UART_CH_USB         _DEF_UART2
#define      HW_UART_CH_NET         _DEF_UART3
#define      HW_UART_CH_CMD         _DEF_UART4    // cmd 패킷 위의 가상 CLI 채널
#define      HW_UART_CH_CLI         HW_UART_CH_SWD


#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    32
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    8
#define      HW_CLI_LINE_BUF_MAX    64

#define _USE_HW_CLI_GUI
#define      HW_CLI_GUI_WIDTH       80
#define      HW_CLI_GUI_HEIGHT      24

#define _USE_HW_LOG
#define      HW_LOG_CH              HW_UART_CH_SWD
#define      HW_LOG_BOOT_BUF_MAX    2048
#define      HW_LOG_LIST_BUF_MAX    4096

#define _USE_HW_SWTIMER
#define      HW_SWTIMER_MAX_CH      16

#define _USE_HW_RESET
//   0 = 앱. 리셋 원인 플래그는 부트로더가 읽고 클리어한 뒤 백업 레지스터에 남긴다.
//   1 로 두면 앱이 이미 지워진 플래그를 읽어 항상 "원인 없음" 이 된다.
#define      HW_RESET_BOOT          0

#define _USE_HW_FLASH

#define _USE_HW_RTC
//   백업 레지스터 배정은 부트로더(stm32h5-boot/src/hw/hw_def.h)와 반드시 동일해야 한다.
#define      HW_RTC_BOOT_MODE       RTC_BKP_DR3
#define      HW_RTC_RESET_BITS      RTC_BKP_DR4
#define      HW_RTC_RESET_CNT       RTC_BKP_DR5
#define      HW_RTC_BOOT_TRY        RTC_BKP_DR6
#define      HW_RTC_FAULT_CNT       RTC_BKP_DR7
#define      HW_RTC_ECC_ADDR        RTC_BKP_DR8

#define      HW_RESET_CNT_MAGIC     0xA55A0000UL
#define      HW_RESET_CNT_MASK      0x000000FFUL
#define      HW_RESET_DBLCLK_MS     300
#define      HW_RESET_DBLCLK_CNT    2
#define      HW_BOOT_TRY_MAX        3
#define      HW_BOOT_FAULT_MAX      3
#define      HW_ECC_MAGIC           0xEC000000UL

//   앱이 이만큼 정상 동작하면 resetConfirmBoot() 으로 부팅 성공을 확정한다.
//   너무 짧으면 "부팅 몇 초 뒤 항상 죽는" 펌웨어를 못 잡고,
//   너무 길면 confirm 전에 전원이 끊길 때 오탐 롤백이 늘어난다.
#define      HW_BOOT_CONFIRM_MS     10000

#define _USE_HW_GPIO
#define      HW_GPIO_MAX_CH         GPIO_PIN_MAX

#define _USE_HW_USB
#define _USE_HW_CDC
#define      HW_USE_CDC             1
#define      HW_USE_MSC             0
#define      HW_USE_HID             1
//   CDC 와 HID 를 한 장치로 묶는다. 웹페이지가 앱 상태에서도 붙을 수 있어야 한다.
#define      HW_USE_CMP             1

//   부트로더와 동일한 VID/PID 로 열거한다. 호스트는 필터 한 벌로 두 상태를 모두
//   잡고, 어느 쪽인지는 BOOT_CMD_INFO 의 HW_DEV_MODE 로 구분한다.
#define      HW_USB_VID             0xCAFE
#define      HW_USB_PID             0xB003

#define _USE_HW_CMD
#define      HW_CMD_MAX_DATA_LENGTH 1024

#define _USE_HW_EVENT
#define      HW_EVENT_Q_MAX         8
#define      HW_EVENT_NODE_MAX      16  

#define _USE_HW_WIZNET
#define      HW_WIZNET_SOCKET_CMD   0
#define      HW_WIZNET_SOCKET_DHCP  1
#define      HW_WIZNET_SOCKET_SNTP  2
#define      HW_WIZNET_SOCKET_TCP   3


//-- Flash Layout
//   부트로더(stm32h5-boot/src/hw/hw_def.h)와 반드시 동일하게 유지할 것
//
#define FLASH_SIZE_TAG              0x400
#define FLASH_SIZE_VEC              0x400
#define FLASH_SIZE_VER              0x400

#define FLASH_ADDR_BOOT             0x08000000
#define FLASH_SIZE_BOOT             (128*1024)
#define FLASH_ADDR_FIRM             0x08020000
#define FLASH_SIZE_FIRM             (448*1024)

#define FLASH_ADDR_SLOT0            0x08100000
#define FLASH_ADDR_SLOT1            0x08170000
#define FLASH_SIZE_SLOT             (448*1024)
#define FLASH_SLOT_MAX              2

#define FLASH_ADDR_BOOT_LOG         0x081E0000
#define FLASH_SIZE_BOOT_LOG         (16*1024)

#define FLASH_ADDR_NVS              0x081E4000
#define FLASH_SIZE_NVS              (112*1024)

#define BOARD_UF2_FAMILY_ID         0xFFFF0003UL

//   호스트(웹/툴)가 지금 붙은 쪽이 부트로더인지 앱인지 구분하는 값.
//   BOOT_CMD_INFO 응답에 실려 나간다. 같은 VID/PID 로 열거되므로 USB 만으로는
//   구분할 수 없다.
#define HW_DEV_MODE_BOOT            0
#define HW_DEV_MODE_APP             1
#define HW_DEV_MODE                 HW_DEV_MODE_APP      // 앱

//   앱은 실행 중인 뱅크1 전체를 보호한다. 갱신용 기록은 뱅크2 슬롯에만 한다.
#define FLASH_PROTECT_ADDR          FLASH_ADDR_BOOT
#define FLASH_PROTECT_SIZE          (FLASH_ADDR_SLOT0 - FLASH_ADDR_BOOT)


//-- CLI
//
#define _USE_CLI_HW_LOG             1
#define _USE_CLI_HW_ASSERT          1
#define _USE_CLI_HW_UART            1
#define _USE_CLI_HW_USB             1
#define _USE_CLI_HW_WIZNET          1
#define _USE_CLI_HW_RESET           1
#define _USE_CLI_HW_FLASH           1
#define _USE_CLI_HW_BOOT            1


typedef enum
{
  W6300_RST,
  W6300_INT,
  W6300_CS,
  GPIO_PIN_MAX
} GpioPinName_t;

#endif
