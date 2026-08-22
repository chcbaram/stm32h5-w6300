#include "fault.h"
#include "reset.h"


#ifdef _USE_HW_FAULT

static __attribute__((section(".noinit")))  fault_log_t fault_log;

//-- 이번 부팅이 폴트 직후인가.
//
//   fault_log 는 .noinit 이라 리셋에도 살아남는다. 그래서 폴트가 한참 전에
//   한 번 났으면 REG_PC 가 그대로 남아, 폴트와 무관한 롤백/검증 실패 기록에도
//   엉뚱한 PC 가 찍혔다. 전원을 막 넣은 직후라면 아예 SRAM 쓰레기값이다.
//
//   faultInit() 이 매직을 보고 한 번만 걷어 담는다. 매직은 그 자리에서 지우므로
//   다음 부팅에는 거짓이 된다.
//
static bool     is_fault_boot = false;
static uint32_t fault_boot_pc = 0;




bool faultInit(void)
{

  if (fault_log.magic_number == 0x5555AAAA)
  {
    fault_log.magic_number = 0;

    is_fault_boot = true;
    fault_boot_pc = fault_log.is_reg ? fault_log.REG_PC : 0;

    logPrintf("Fault Message\n");
    logPrintf("  Type : %d\n",     fault_log.type);
    logPrintf("  Msg  : %s\n",     fault_log.msg);

    if (fault_log.is_reg == true)
    {
      logPrintf("  R0   : 0x%08X\n", fault_log.REG_R0);
      logPrintf("  R1   : 0x%08X\n", fault_log.REG_R1);
      logPrintf("  R2   : 0x%08X\n", fault_log.REG_R2);
      logPrintf("  R3   : 0x%08X\n", fault_log.REG_R3);
      logPrintf("  R12  : 0x%08X\n", fault_log.REG_R12);
      logPrintf("  LR   : 0x%08X\n", fault_log.REG_LR);
      logPrintf("  PC   : 0x%08X\n", fault_log.REG_PC);
      logPrintf("  PSR  : 0x%08X\n", fault_log.REG_PSR);
    }
    logPrintf("\n");
  }

  return true;
}

bool faultReset(const char *p_msg, uint32_t *p_stack)
{
  fault_log.magic_number = 0x5555AAAA;
  fault_log.type = 0;
  
  if (p_stack != NULL)
  {
    fault_log.is_reg  = true; 
    fault_log.REG_R0  = p_stack[0];  // Register R0
    fault_log.REG_R1  = p_stack[1];  // Register R1
    fault_log.REG_R2  = p_stack[2];  // Register R2
    fault_log.REG_R3  = p_stack[3];  // Register R3
    fault_log.REG_R12 = p_stack[4];  // Register R12
    fault_log.REG_LR  = p_stack[5];  // Link register LR
    fault_log.REG_PC  = p_stack[6];  // Program counter PC
    fault_log.REG_PSR = p_stack[7];  // Program status word PSR

    // TODO: 일단 SRAM에서 실행된 코드는 무시
    //
    if (fault_log.REG_PC >= SRAM1_BASE && fault_log.REG_PC < (SRAM1_BASE + 640*1024))
    {
      fault_log.magic_number = 0;
    }
  }
  else
  {
    fault_log.is_reg = false;
  }

  uint32_t i;
  bool last = false;
  for (i=0; i<sizeof(fault_log.msg)-1; i++)
  {
    
    if (p_msg[i] != 0 && last == false)
    {
      fault_log.msg[i] = p_msg[i];
    }
    else
    {
      last = true;
      fault_log.msg[i] = 0;
    }
  }


  // 폴트로 인한 리셋 횟수를 백업 레지스터에 누적한다.
  // .noinit(SRAM) 은 전원이 끊기면 사라지므로 여기에 따로 남긴다.
  resetIncFaultCount();

  NVIC_SystemReset();

  return true;
}

//-- 부트 이벤트 로그에 폴트 PC 를 남기기 위한 접근자.
//
bool faultGetLog(fault_log_t *p_log)
{
  if (p_log == NULL)
    return false;

  *p_log = fault_log;
  return is_fault_boot;
}

bool faultIsFaultBoot(void)
{
  return is_fault_boot;
}

//   폴트 직후 부팅이 아니면 0 이다. 부트 이벤트 로그의 PC 열은 이 값을 쓰므로,
//   폴트가 원인이 아닌 기록에는 '-' 가 찍힌다.
uint32_t faultGetPc(void)
{
  return is_fault_boot ? fault_boot_pc : 0;
}

#endif