/* Includes ------------------------------------------------------------------*/
#include "bsp.h"
#include "fault.h"
#include "rtc.h"
#include "stm32h5xx_it.h"
#include "hw_def.h"


/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  // STM32H5 는 플래시 ECC 2비트 오류를 NMI 로 올린다.
  //
  // 여기서 while(1) 로 멈추면 "복구 수단인 부트로더가 복구 불가 상태"가 된다.
  // 실기에서 재현됨: 이미 프로그램된 쿼드워드를 다시 쓰면(HAL 은 OK 를 반환)
  // ECC 가 깨지고, 그 워드를 읽는 순간 여기로 들어와 영구 정지했다.
  //
  // 따라서 절대 멈추지 않는다. 오류 위치를 백업 레지스터에 남기고 리셋하면,
  // 다음 부팅에서 bootUp() 이 해당 영역을 정리한다.
  //
#if defined(_USE_HW_RTC) && defined(HW_RTC_ECC_ADDR)
  if (FLASH->ECCDETR & FLASH_ECCR_ECCD)
  {
    uint32_t addr_ecc = FLASH->ECCDETR & FLASH_ECCR_ADDR_ECC;
    uint32_t bank     = (FLASH->ECCDETR & FLASH_ECCR_BK_ECC) ? 1 : 0;

    FLASH->ECCDETR |= FLASH_ECCR_ECCD;    // rc_w1 : 플래그 클리어

    rtcSetReg(HW_RTC_ECC_ADDR, HW_ECC_MAGIC | (bank << 20) | addr_ecc);
  }
#endif

  NVIC_SystemReset();
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler_C(uint32_t *p_stack)
{
  faultReset("HardFault", p_stack);
  while (1)
  {
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler_C(uint32_t *p_stack)
{
  faultReset("MemManage", p_stack);
  while (1)
  {
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler_C(uint32_t *p_stack)
{
  faultReset("BusFault", p_stack);
  while (1)
  {
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler_C(uint32_t *p_stack)
{
  faultReset("UsageFault", p_stack);
  while (1)
  {
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
#ifndef _USE_HW_RTOS
void SVC_Handler(void)
{
}
#endif

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief This function handles Pendable request for system service.
  */
#ifndef _USE_HW_RTOS
void PendSV_Handler(void)
{
}
#endif

#ifdef _USE_HW_RTOS
extern void osSystickHandler(void);

void SysTick_Handler(void)
{
  osSystickHandler();
}
#else
#ifdef _USE_HW_SWTIMER
extern void swtimerISR(void);
#endif

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
#ifdef _USE_HW_SWTIMER
  swtimerISR();
#endif
}
#endif
