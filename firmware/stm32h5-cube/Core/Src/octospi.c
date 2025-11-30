/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    octospi.c
  * @brief   This file provides code for the configuration
  *          of the OCTOSPI instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "octospi.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

XSPI_HandleTypeDef hospi1;
DMA_HandleTypeDef handle_GPDMA1_Channel1;

/* OCTOSPI1 init function */
void MX_OCTOSPI1_Init(void)
{

  /* USER CODE BEGIN OCTOSPI1_Init 0 */

  /* USER CODE END OCTOSPI1_Init 0 */

  /* USER CODE BEGIN OCTOSPI1_Init 1 */

  /* USER CODE END OCTOSPI1_Init 1 */
  hospi1.Instance = OCTOSPI1;
  hospi1.Init.FifoThresholdByte = 1;
  hospi1.Init.MemoryMode = HAL_XSPI_SINGLE_MEM;
  hospi1.Init.MemoryType = HAL_XSPI_MEMTYPE_MICRON;
  hospi1.Init.MemorySize = HAL_XSPI_SIZE_16B;
  hospi1.Init.ChipSelectHighTimeCycle = 1;
  hospi1.Init.FreeRunningClock = HAL_XSPI_FREERUNCLK_DISABLE;
  hospi1.Init.ClockMode = HAL_XSPI_CLOCK_MODE_0;
  hospi1.Init.WrapSize = HAL_XSPI_WRAP_NOT_SUPPORTED;
  hospi1.Init.ClockPrescaler = 0;
  hospi1.Init.SampleShifting = HAL_XSPI_SAMPLE_SHIFT_NONE;
  hospi1.Init.DelayHoldQuarterCycle = HAL_XSPI_DHQC_DISABLE;
  hospi1.Init.ChipSelectBoundary = HAL_XSPI_BONDARYOF_NONE;
  hospi1.Init.DelayBlockBypass = HAL_XSPI_DELAY_BLOCK_BYPASS;
  hospi1.Init.Refresh = 0;
  if (HAL_XSPI_Init(&hospi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OCTOSPI1_Init 2 */

  /* USER CODE END OCTOSPI1_Init 2 */

}

void HAL_XSPI_MspInit(XSPI_HandleTypeDef* xspiHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(xspiHandle->Instance==OCTOSPI1)
  {
  /* USER CODE BEGIN OCTOSPI1_MspInit 0 */

  /* USER CODE END OCTOSPI1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_OSPI;
    PeriphClkInitStruct.OspiClockSelection = RCC_OSPICLKSOURCE_PLL1Q;
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

    /* OCTOSPI1 DMA Init */
    /* GPDMA1_REQUEST_OCTOSPI1 Init */
    handle_GPDMA1_Channel1.Instance = GPDMA1_Channel1;
    handle_GPDMA1_Channel1.Init.Request = GPDMA1_REQUEST_OCTOSPI1;
    handle_GPDMA1_Channel1.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    handle_GPDMA1_Channel1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    handle_GPDMA1_Channel1.Init.SrcInc = DMA_SINC_FIXED;
    handle_GPDMA1_Channel1.Init.DestInc = DMA_DINC_INCREMENTED;
    handle_GPDMA1_Channel1.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    handle_GPDMA1_Channel1.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    handle_GPDMA1_Channel1.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
    handle_GPDMA1_Channel1.Init.SrcBurstLength = 1;
    handle_GPDMA1_Channel1.Init.DestBurstLength = 1;
    handle_GPDMA1_Channel1.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
    handle_GPDMA1_Channel1.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    handle_GPDMA1_Channel1.Init.Mode = DMA_NORMAL;
    if (HAL_DMA_Init(&handle_GPDMA1_Channel1) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(xspiHandle, hdmarx, handle_GPDMA1_Channel1);

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel1, DMA_CHANNEL_NPRIV) != HAL_OK)
    {
      Error_Handler();
    }

    /* OCTOSPI1 interrupt Init */
    HAL_NVIC_SetPriority(OCTOSPI1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(OCTOSPI1_IRQn);
  /* USER CODE BEGIN OCTOSPI1_MspInit 1 */

  /* USER CODE END OCTOSPI1_MspInit 1 */
  }
}

void HAL_XSPI_MspDeInit(XSPI_HandleTypeDef* xspiHandle)
{

  if(xspiHandle->Instance==OCTOSPI1)
  {
  /* USER CODE BEGIN OCTOSPI1_MspDeInit 0 */

  /* USER CODE END OCTOSPI1_MspDeInit 0 */
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

    /* OCTOSPI1 DMA DeInit */
    HAL_DMA_DeInit(xspiHandle->hdmarx);

    /* OCTOSPI1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(OCTOSPI1_IRQn);
  /* USER CODE BEGIN OCTOSPI1_MspDeInit 1 */

  /* USER CODE END OCTOSPI1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
