/**
 ******************************************************************************
 * @file    stm32f4xx_hal_msp_template.c
 * @author  MCD Application Team
 * @brief   This file contains the HAL System and Peripheral (PPP) MSP
 * initialization and de-initialization functions. It should be copied to the
 * application folder and renamed into 'stm32f4xx_hal_msp.c'.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "es_wifi_conf.h"

/** @addtogroup STM32F4xx_HAL_Driver
 * @{
 */

/** @defgroup HAL_MSP HAL MSP
 * @brief HAL MSP module.
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/** @defgroup HAL_MSP_Private_Functions HAL MSP Private Functions
 * @{
 */

/**
 * @brief  Initializes the Global MSP.
 * @note   This function is called from HAL_Init() function to perform system
 *         level initialization (GPIOs, clock, DMA, interrupt).
 * @retval None
 */
void HAL_MspInit(void) {
}

/**
 * @brief  DeInitializes the Global MSP.
 * @note   This functiona is called from HAL_DeInit() function to perform system
 *         level de-initialization (GPIOs, clock, DMA, interrupt).
 * @retval None
 */
void HAL_MspDeInit(void) {
}

/**
 * @brief  Initializes the SPI MSP for WiFi module.
 * @param  hspi: SPI handle pointer
 * @retval None
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    if(hspi->Instance == SPI3) {
        /* Enable peripheral clocks */
        WIFI_SPI_CLK_ENABLE();
        WIFI_SPI_SCK_GPIO_CLK_ENABLE();
        WIFI_SPI_MISO_GPIO_CLK_ENABLE();
        WIFI_SPI_MOSI_GPIO_CLK_ENABLE();
        WIFI_CS_GPIO_CLK_ENABLE();
        WIFI_RESET_GPIO_CLK_ENABLE();
        WIFI_WAKEUP_GPIO_CLK_ENABLE();
        WIFI_DATAREADY_GPIO_CLK_ENABLE();
        
        /* Configure SPI SCK pin (PB3) */
        GPIO_InitStruct.Pin = WIFI_SPI_SCK_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = WIFI_SPI_SCK_AF;
        HAL_GPIO_Init(WIFI_SPI_SCK_GPIO_PORT, &GPIO_InitStruct);
        
        /* Configure SPI MISO pin (PB4) */
        GPIO_InitStruct.Pin = WIFI_SPI_MISO_PIN;
        GPIO_InitStruct.Alternate = WIFI_SPI_MISO_AF;
        HAL_GPIO_Init(WIFI_SPI_MISO_GPIO_PORT, &GPIO_InitStruct);
        
        /* Configure SPI MOSI pin (PB5) */
        GPIO_InitStruct.Pin = WIFI_SPI_MOSI_PIN;
        GPIO_InitStruct.Alternate = WIFI_SPI_MOSI_AF;
        HAL_GPIO_Init(WIFI_SPI_MOSI_GPIO_PORT, &GPIO_InitStruct);
        
        /* Configure CS pin (PE0) - Output, initially high */
        GPIO_InitStruct.Pin = WIFI_CS_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = 0;
        HAL_GPIO_Init(WIFI_CS_GPIO_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(WIFI_CS_GPIO_PORT, WIFI_CS_PIN, GPIO_PIN_SET);
        
        /* Configure RESET pin (PE8) - Output, initially high (not in reset) */
        GPIO_InitStruct.Pin = WIFI_RESET_PIN;
        HAL_GPIO_Init(WIFI_RESET_GPIO_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(WIFI_RESET_GPIO_PORT, WIFI_RESET_PIN, GPIO_PIN_SET);
        
        /* Configure WAKEUP pin (PB15) - Output, initially low */
        GPIO_InitStruct.Pin = WIFI_WAKEUP_PIN;
        HAL_GPIO_Init(WIFI_WAKEUP_GPIO_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(WIFI_WAKEUP_GPIO_PORT, WIFI_WAKEUP_PIN, GPIO_PIN_RESET);
        
        /* Configure DATA_READY pin (PE1) - Input */
        GPIO_InitStruct.Pin = WIFI_DATAREADY_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(WIFI_DATAREADY_GPIO_PORT, &GPIO_InitStruct);
    }
}

/**
 * @brief  DeInitializes the SPI MSP for WiFi module.
 * @param  hspi: SPI handle pointer
 * @retval None
 */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi) {
    if(hspi->Instance == SPI3) {
        /* Disable SPI clock */
        WIFI_SPI_CLK_DISABLE();
        
        /* DeInitialize GPIO pins */
        HAL_GPIO_DeInit(WIFI_SPI_SCK_GPIO_PORT, WIFI_SPI_SCK_PIN);
        HAL_GPIO_DeInit(WIFI_SPI_MISO_GPIO_PORT, WIFI_SPI_MISO_PIN);
        HAL_GPIO_DeInit(WIFI_SPI_MOSI_GPIO_PORT, WIFI_SPI_MOSI_PIN);
        HAL_GPIO_DeInit(WIFI_CS_GPIO_PORT, WIFI_CS_PIN);
        HAL_GPIO_DeInit(WIFI_RESET_GPIO_PORT, WIFI_RESET_PIN);
        HAL_GPIO_DeInit(WIFI_WAKEUP_GPIO_PORT, WIFI_WAKEUP_PIN);
        HAL_GPIO_DeInit(WIFI_DATAREADY_GPIO_PORT, WIFI_DATAREADY_PIN);
    }
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
