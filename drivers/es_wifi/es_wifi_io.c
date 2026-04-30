/**
  ******************************************************************************
  * @file    es_wifi_io.c
  * @author  MCD Application Team
  * @brief   This file implements the IO operations to deal with the es-wifi
  *          module. It mainly Inits and Deinits the SPI interface. Send and
  *          receive data over it.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "es_wifi_io.h"
#include <string.h>
#include "diag/trace.h"

/* Private define ------------------------------------------------------------*/
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
/* Private typedef -----------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
                       COM Driver Interface (SPI)
*******************************************************************************/
/**
  * @brief  Initialize SPI MSP
  * @param  hspi: SPI handle
  * @retval None
  */
void SPI_WIFI_MspInit(SPI_HandleTypeDef* hspi)
{
  
  GPIO_InitTypeDef GPIO_Init;
  
  /* Enable clocks for SPI and GPIO ports */
  __HAL_RCC_SPI3_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  
  /* Configure Wakeup pin (GPIOB, PIN_15) */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
  GPIO_Init.Pin       = GPIO_PIN_15;
  GPIO_Init.Mode      = GPIO_MODE_OUTPUT_PP;
  GPIO_Init.Pull      = GPIO_NOPULL;
  GPIO_Init.Speed     = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_Init);

  /* Configure Data Ready pin (GPIOG, PIN_12) */
  GPIO_Init.Pin       = GPIO_PIN_12;
  GPIO_Init.Mode      = GPIO_MODE_IT_RISING;
  GPIO_Init.Pull      = GPIO_NOPULL;
  GPIO_Init.Speed     = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_Init);

  /* Configure Reset pin (GPIOH, PIN_1) */
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_SET);
  GPIO_Init.Pin       = GPIO_PIN_1;
  GPIO_Init.Mode      = GPIO_MODE_OUTPUT_PP;
  GPIO_Init.Pull      = GPIO_NOPULL;
  GPIO_Init.Speed     = GPIO_SPEED_FREQ_LOW;
  GPIO_Init.Alternate = 0;
  HAL_GPIO_Init(GPIOH, &GPIO_Init);
  
  /* Configure SPI CS/NSS pin (GPIOG, PIN_11) */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_11, GPIO_PIN_SET);
  GPIO_Init.Pin       = GPIO_PIN_11;
  GPIO_Init.Mode      = GPIO_MODE_OUTPUT_PP;
  GPIO_Init.Pull      = GPIO_NOPULL;
  GPIO_Init.Speed     = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOG, &GPIO_Init);
  
  /* Configure SPI SCK pin (GPIOB, PIN_12) */
  GPIO_Init.Pin       = GPIO_PIN_12;
  GPIO_Init.Mode      = GPIO_MODE_AF_PP;
  GPIO_Init.Pull      = GPIO_NOPULL;
  GPIO_Init.Speed     = GPIO_SPEED_FREQ_MEDIUM;
  GPIO_Init.Alternate = GPIO_AF7_SPI3;
  HAL_GPIO_Init(GPIOB, &GPIO_Init);
  
  /* Configure SPI MOSI pin (GPIOB, PIN_5) */
  GPIO_Init.Pin       = GPIO_PIN_5;
  GPIO_Init.Mode      = GPIO_MODE_AF_PP;
  GPIO_Init.Pull      = GPIO_NOPULL;
  GPIO_Init.Speed     = GPIO_SPEED_FREQ_MEDIUM;
  GPIO_Init.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(GPIOB, &GPIO_Init);
  
  /* Configure SPI MISO pin (GPIOB, PIN_4) */
  GPIO_Init.Pin       = GPIO_PIN_4;
  GPIO_Init.Mode      = GPIO_MODE_AF_PP;
  GPIO_Init.Pull      = GPIO_PULLUP;
  GPIO_Init.Speed     = GPIO_SPEED_FREQ_MEDIUM;
  GPIO_Init.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(GPIOB, &GPIO_Init);
}

/**
  * @brief  Initialize the SPI3
  * @param  None
  * @retval None
  */
int8_t SPI_WIFI_Init(void)
{
  uint32_t tickstart = HAL_GetTick();
  uint8_t Prompt[6];
  uint8_t count = 0;
  HAL_StatusTypeDef  Status;
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Starting SPI initialization...\n");
  #endif
  
  hspi.Instance               = SPI3;
  SPI_WIFI_MspInit(&hspi);
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] GPIO pins configured\n");
  #endif
  
  hspi.Init.Mode              = SPI_MODE_MASTER;
  hspi.Init.Direction         = SPI_DIRECTION_2LINES;
  hspi.Init.DataSize          = SPI_DATASIZE_16BIT;
  hspi.Init.CLKPolarity       = SPI_POLARITY_LOW;
  hspi.Init.CLKPhase          = SPI_PHASE_1EDGE;
  hspi.Init.NSS               = SPI_NSS_SOFT;
  hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  /* Slower: 16MHz/8 = 2MHz for reliability */
  hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
  hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
  hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
  hspi.Init.CRCPolynomial     = 0;
  
  if (HAL_SPI_Init(&hspi) != HAL_OK)
  {
    #ifdef TRACE
    trace_printf("  [SPI_WIFI_Init] ERROR: HAL_SPI_Init failed!\n");
    #endif
    return -1;
  }
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] SPI peripheral initialized\n");
  trace_printf("  [SPI_WIFI_Init] Resetting WiFi module...\n");
  trace_printf("  [SPI_WIFI_Init] Setting reset pin LOW...\n");
  #endif
  
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_RESET);
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Waiting 10ms...\n");
  #endif
  
  HAL_Delay(10);
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Setting reset pin HIGH...\n");
  #endif
  
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_SET);
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Waiting 500ms for module boot...\n");
  #endif
  
  HAL_Delay(500);
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Reset complete, enabling NSS...\n");
  #endif
  
  WIFI_ENABLE_NSS();
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] NSS enabled, checking data ready pin...\n");
  trace_printf("  [SPI_WIFI_Init] Data ready pin state: %d\n", HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_12));
  #endif
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Waiting for module ready (data ready pin)...\n");
  #endif
  
  tickstart = HAL_GetTick();
  uint32_t wait_count = 0;
  
  /* Wait for data ready pin to go high (module has data) */
  while (!WIFI_IS_CMDDATA_READY())
  {
    wait_count++;
    if((HAL_GetTick() - tickstart) > 5000)  /* 5 second timeout */
    {
      #ifdef TRACE
      trace_printf("  [SPI_WIFI_Init] ERROR: Timeout waiting for data ready pin (waited %lu ms)\n",
                   HAL_GetTick() - tickstart);
      trace_printf("  [SPI_WIFI_Init] Data ready pin final state: %d\n", HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_12));
      #endif
      WIFI_DISABLE_NSS();
      return -1;
    }
    if(wait_count % 100 == 0)
    {
      HAL_Delay(1);  /* Small delay to avoid busy waiting */
    }
  }
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Data ready! Reading prompt...\n");
  #endif
  
  /* Now read the prompt */
  tickstart = HAL_GetTick();
  while (WIFI_IS_CMDDATA_READY() && count < 6)
  {
    Status = HAL_SPI_Receive(&hspi , &Prompt[count], 1, 1000);
    count += 2;
    if(((HAL_GetTick() - tickstart ) > 5000) || (Status != HAL_OK))
    {
      #ifdef TRACE
      trace_printf("  [SPI_WIFI_Init] ERROR: Timeout or SPI error reading prompt (count=%d, status=%d)\n", count, Status);
      #endif
      WIFI_DISABLE_NSS();
      return -1;
    }
  }
  
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Received %d bytes, validating prompt...\n", count);
  trace_printf("  [SPI_WIFI_Init] Prompt: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
               Prompt[0], Prompt[1], Prompt[2], Prompt[3], Prompt[4], Prompt[5]);
  #endif
  
  if((Prompt[0] != 0x15) ||(Prompt[1] != 0x15) ||(Prompt[2] != '\r')||
       (Prompt[3] != '\n') ||(Prompt[4] != '>') ||(Prompt[5] != ' '))
  {
    #ifdef TRACE
    trace_printf("  [SPI_WIFI_Init] ERROR: Invalid prompt received!\n");
    #endif
    WIFI_DISABLE_NSS();
    return -1;
  }
   
  #ifdef TRACE
  trace_printf("  [SPI_WIFI_Init] Prompt validated successfully\n");
  #endif
  
  WIFI_DISABLE_NSS();
  return 0;
}

/**
  * @brief  Initialize the UART1
  * @param  None
  * @retval None
  */
int8_t SPI_WIFI_DeInit(void)
{
  HAL_SPI_DeInit( &hspi );
  return 0;
}

/**
  * @brief  Receive wifi Data from SPI
  * @param  Pdata : pointer to data
  * @retval Length of received data (payload)
  */
int16_t SPI_WIFI_ReceiveData(uint8_t *pData, uint16_t len, uint32_t timeout)
{
  uint32_t tickstart = HAL_GetTick();
  int16_t length = 0;
   
  WIFI_DISABLE_NSS(); 
  
  while (!WIFI_IS_CMDDATA_READY())
  {
    if((HAL_GetTick() - tickstart ) > timeout)
    {
      return -1;
    }
  }
  
  WIFI_ENABLE_NSS(); 
  
  while (WIFI_IS_CMDDATA_READY())
  {
    if((length < len) || (!len))
    {
      HAL_SPI_Receive(&hspi, pData, 1, timeout) ;
      length += 2;
      pData  += 2;
      
      if((HAL_GetTick() - tickstart ) > timeout)
      {
        WIFI_DISABLE_NSS(); 
        return -1;
      }
    }
    else
    {
      break;
    }
  }
  
  if(*(--pData) == 0x15)
  {
    length--;
  }
  
  WIFI_DISABLE_NSS(); 
  return length;
}

/**
  * @brief  Send wifi Data thru SPI
  * @param  Pdata : pointer to data
  * @retval Length of sent data
  */
int16_t SPI_WIFI_SendData( uint8_t *pdata,  uint16_t len, uint32_t timeout)
{
  uint32_t tickstart = HAL_GetTick();
   
  while (!WIFI_IS_CMDDATA_READY())
  {
    if((HAL_GetTick() - tickstart ) > timeout)
    {
      return -1;
    }
  }
  
  WIFI_ENABLE_NSS(); 
  
  if(len & 0x1) 
  {
    pdata[len] = '\n';
  }
  if( HAL_SPI_Transmit(&hspi, (uint8_t *)pdata , (len+1)/2, timeout) != HAL_OK)
  {
    return -1;
  }
  pdata[len] = 0;  
  return len;
}

/**
  * @brief  Delay
  * @param  Delay in ms
  * @retval None
  */
void SPI_WIFI_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}
