#ifndef HW_DEF_H_
#define HW_DEF_H_



#include "bsp.h"
#include "assert_def.h"


#define _DEF_FIRMWATRE_VERSION    "V260822R1"
#define _DEF_BOARD_NAME           "STM32H5-W6300-BOOT"



#define _USE_HW_ASSERT
#define _USE_HW_FAULT
#define _USE_HW_FLASH


#define _USE_HW_LED
#define      HW_LED_MAX_CH          1

#define _USE_HW_UART
#define      HW_UART_MAX_CH         2
#define      HW_UART_CH_SWD         _DEF_UART1
#define      HW_UART_CH_USB         _DEF_UART2
#define      HW_UART_CH_CLI         HW_UART_CH_SWD


#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    24
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    4
#define      HW_CLI_LINE_BUF_MAX    64

#define _USE_HW_LOG
#define      HW_LOG_CH              HW_UART_CH_SWD
#define      HW_LOG_BOOT_BUF_MAX    1024
#define      HW_LOG_LIST_BUF_MAX    1024

#define _USE_HW_RESET
#define      HW_RESET_BOOT          1

#define _USE_HW_RTC
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
#define      HW_BOOT_CONFIRM_MS     10000
#define      HW_ECC_MAGIC           0xEC000000UL

#define _USE_HW_GPIO
#define      HW_GPIO_MAX_CH         GPIO_PIN_MAX

#define _USE_HW_CMD
#define      HW_CMD_MAX_DATA_LENGTH 1024

#define _USE_HW_USB
#define _USE_HW_CDC
#define      HW_USE_CDC             1
#define      HW_USE_MSC             1
#define      HW_USE_HID             1


//-- Flash Layout
//   앱(stm32h5-fw)의 hw_def.h 와 반드시 동일하게 유지할 것
//
#define FLASH_SIZE_TAG              0x400
#define FLASH_SIZE_VEC              0x400
#define FLASH_SIZE_VER              0x400

//   Bank1 (0x08000000 ~ 0x080FFFFF) : 앱이 실행되는 뱅크
#define FLASH_ADDR_BOOT             0x08000000
#define FLASH_SIZE_BOOT             (128*1024)
#define FLASH_ADDR_FIRM             0x08020000
#define FLASH_SIZE_FIRM             (448*1024)

//   Bank2 (0x08100000 ~ 0x081FFFFF) : 앱 실행 중에도 RWW 로 안전하게 쓰기 가능
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
#define HW_DEV_MODE                 HW_DEV_MODE_BOOT      // 부트로더

//   flash.c 가 쓰기를 거부할 영역.
//   부트로더는 자기 자신만 보호한다. FIRM 은 갱신해야 하기 때문이다.
#define FLASH_PROTECT_ADDR          FLASH_ADDR_BOOT
#define FLASH_PROTECT_SIZE          FLASH_SIZE_BOOT


//-- CLI
//
#define _USE_CLI_HW_LOG             1
#define _USE_CLI_HW_ASSERT          1
#define _USE_CLI_HW_UART            1
#define _USE_CLI_HW_USB             1
#define _USE_CLI_HW_FLASH           1
#define _USE_CLI_HW_RESET           1
#define _USE_CLI_HW_GPIO            1
#define _USE_CLI_HW_BOOT            1


typedef enum
{
  W6300_RST,
  W6300_INT,
  W6300_CS,
  GPIO_PIN_MAX
} GpioPinName_t;

#endif
