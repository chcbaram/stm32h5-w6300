#include "wiz_spi.h"



#ifdef _USE_HW_WIZSPI
#include "cli.h"
#include "gpio.h"



static XSPI_HandleTypeDef hospi1;


static bool wizspiInitHw(void);
#ifdef _USE_HW_CLI
static void cliCmd(cli_args_t *args);
#endif




bool wizspiInit(void)
{
  bool ret = true;

  ret &= wizspiInitHw();

  logPrintf("[%s] wizspiInit()\n", ret ? "OK":"E_");

#ifdef _USE_HW_CLI
  cliAdd("wizspi", cliCmd);
#endif

  return ret;
}

bool wizspiInitHw(void)
{
  bool ret = true;


  gpioPinWrite(W6300_RST, _DEF_LOW);
  delay(5);
  gpioPinWrite(W6300_RST, _DEF_HIGH);
  delay(70);  

  // 250Mhz / (1+3) = 62.5Mhz
  //
  hospi1.Instance                     = OCTOSPI1;
  hospi1.Init.FifoThresholdByte       = 1;
  hospi1.Init.MemoryMode              = HAL_XSPI_SINGLE_MEM;
  hospi1.Init.MemoryType              = HAL_XSPI_MEMTYPE_MICRON;
  hospi1.Init.MemorySize              = HAL_XSPI_SIZE_512KB;
  hospi1.Init.ChipSelectHighTimeCycle = 1;
  hospi1.Init.FreeRunningClock        = HAL_XSPI_FREERUNCLK_DISABLE;
  hospi1.Init.ClockMode               = HAL_XSPI_CLOCK_MODE_0;
  hospi1.Init.WrapSize                = HAL_XSPI_WRAP_NOT_SUPPORTED;
  hospi1.Init.ClockPrescaler          = 3; 
  hospi1.Init.SampleShifting          = HAL_XSPI_SAMPLE_SHIFT_NONE;
  hospi1.Init.DelayHoldQuarterCycle   = HAL_XSPI_DHQC_DISABLE;
  hospi1.Init.ChipSelectBoundary      = HAL_XSPI_BONDARYOF_NONE;
  hospi1.Init.DelayBlockBypass        = HAL_XSPI_DELAY_BLOCK_BYPASS;
  hospi1.Init.Refresh                 = 0;
  if (HAL_XSPI_Init(&hospi1) != HAL_OK)
  {
    logPrintf("[E_] HAL_XSPI_Init()\n");
    ret = false;
  }

  return ret;
}

bool wizspiRead(uint8_t block_sel, uint16_t addr, void *p_data, uint32_t length, uint32_t timeout_ms)
{
  XSPI_RegularCmdTypeDef s_command = {0};
  uint8_t inst;


  inst = (2<<6) | (0<<5) | block_sel;

  /* Initialize the read command */
  s_command.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  
  s_command.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  s_command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  s_command.Instruction        = inst;
  
  s_command.AddressMode        = HAL_XSPI_ADDRESS_4_LINES;
  s_command.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth       = HAL_XSPI_ADDRESS_16_BITS;

  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;

  s_command.DataMode           = HAL_XSPI_DATA_4_LINES;
  s_command.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles        = 2;
  s_command.DQSMode            = HAL_XSPI_DQS_DISABLE;

  s_command.Address            = addr;
  s_command.DataLength         = length;


  /* Send the command */
  if (HAL_XSPI_Command(&hospi1, &s_command, timeout_ms) != HAL_OK)
  {
    return false;
  }

  /* Reception of the data */
  if (HAL_XSPI_Receive(&hospi1, p_data, timeout_ms) != HAL_OK)
  {
    return false;
  }

  return true;
}

bool wizspiWrite(uint8_t block_sel, uint16_t addr, void *p_data, uint32_t length, uint32_t timeout_ms)
{
  XSPI_RegularCmdTypeDef s_command = {0};
  uint8_t inst;

  inst = (2<<6) | (1<<5) | block_sel;


  /* Initialize the program command */
  s_command.OperationType      = HAL_XSPI_OPTYPE_COMMON_CFG;
  s_command.InstructionMode    = HAL_XSPI_INSTRUCTION_1_LINE;
  s_command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  s_command.InstructionWidth   = HAL_XSPI_INSTRUCTION_8_BITS;
  s_command.Instruction        = inst;
  
  s_command.AddressMode        = HAL_XSPI_ADDRESS_4_LINES;
  s_command.AddressDTRMode     = HAL_XSPI_ADDRESS_DTR_DISABLE;
  s_command.AddressWidth       = HAL_XSPI_ADDRESS_16_BITS;
  
  s_command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  
  s_command.DataMode           = HAL_XSPI_DATA_4_LINES;
  s_command.DataDTRMode        = HAL_XSPI_DATA_DTR_DISABLE;
  s_command.DummyCycles        = 2;
  s_command.DQSMode            = HAL_XSPI_DQS_DISABLE;

  s_command.Address            = addr;
  s_command.DataLength         = length;


  if (HAL_XSPI_Command(&hospi1, &s_command, timeout_ms) != HAL_OK)
  {
    return false;
  }

  if (HAL_XSPI_Transmit(&hospi1, p_data, timeout_ms) != HAL_OK)
  {
    return false;
  }

  return true;
}

void HAL_XSPI_MspInit(XSPI_HandleTypeDef* xspiHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(xspiHandle->Instance==OCTOSPI1)
  {
  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_OSPI;
    PeriphClkInitStruct.OspiClockSelection = RCC_OSPICLKSOURCE_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* OCTOSPI1 clock enable */
    __HAL_RCC_OSPI1_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**OCTOSPI1 GPIO Configuration
    PC2     ------> OCTOSPI1_IO2
    PC3     ------> OCTOSPI1_IO0
    PA1     ------> OCTOSPI1_IO3
    PA3     ------> OCTOSPI1_CLK
    PB0     ------> OCTOSPI1_IO1
    PB10     ------> OCTOSPI1_NCS
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_OCTOSPI1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_OCTOSPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF3_OCTOSPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_OCTOSPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_OCTOSPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

void HAL_XSPI_MspDeInit(XSPI_HandleTypeDef* xspiHandle)
{

  if(xspiHandle->Instance==OCTOSPI1)
  {
    /* Peripheral clock disable */
    __HAL_RCC_OSPI1_CLK_DISABLE();

    /**OCTOSPI1 GPIO Configuration
    PC2     ------> OCTOSPI1_IO2
    PC3     ------> OCTOSPI1_IO0
    PA1     ------> OCTOSPI1_IO3
    PA3     ------> OCTOSPI1_CLK
    PB0     ------> OCTOSPI1_IO1
    PB10     ------> OCTOSPI1_NCS
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_2|GPIO_PIN_3);

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1|GPIO_PIN_3);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0|GPIO_PIN_10);
  }
}

#ifdef _USE_HW_CLI
void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    ret = true;
  }

  if (args->argc == 4 && args->isStr(0, "read") == true)
  {
    uint8_t  block_sel;
    uint16_t addr;
    uint16_t length;    
    uint8_t  data_buf[32];

    block_sel = args->getData(1);
    addr      = args->getData(2);
    length    = constrain(args->getData(3), 0, 32);
    
    cliPrintf("bsb    : [4:2] %d, [1:0] %d\n", block_sel>>2 & 0x03, block_sel>>0 & 0x03);
    cliPrintf("addr   : 0x%04X\n", addr);
    cliPrintf("length : %d\n", length);

    if (wizspiRead(block_sel, addr, data_buf, length, 100))
    {
      for (int i=0; i<length; i++)
      {
        cliPrintf("0x%04X : 0x%02X (%d)\n", addr + i, data_buf[i], data_buf[i]);
      }
    }
    else
    {
      cliPrintf("wizspiRead() Fail\n");
    }

    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("wizspi info\n");
    cliPrintf("wizspi read  block_sel addr length\n");
    cliPrintf("wizspi write block_sel addr data\n");
  }
}
#endif


#endif