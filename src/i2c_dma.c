/**
 ******************************************************************************
 * @file    i2c_dma.c
 * @brief   I2C with DMA support driver implementation
 * @author  ApBDC Team
 ******************************************************************************
 * @attention
 *
 * This module provides I2C communication with DMA support for efficient
 * data transfer. It handles initialization, read/write operations, and
 * interrupt callbacks.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "i2c_dma.h"
#include "diag/trace.h"

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef I2cHandle;
DMA_HandleTypeDef hdma_i2c_tx;
DMA_HandleTypeDef hdma_i2c_rx;

static volatile uint8_t i2c_tx_complete = 0;
static volatile uint8_t i2c_rx_complete = 0;

static uint8_t i2c_write_single_buf = 0;
static uint8_t i2c_read_single_buf = 0;

/* Private function prototypes -----------------------------------------------*/
static void I2C_DMA_MspInit(I2C_HandleTypeDef *hi2c);
static void I2C_DMA_ErrorHandler(void);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Initialize I2C peripheral with DMA support
 * @retval None
 */
void I2C_DMA_Init(void) {
    if (HAL_I2C_GetState(&I2cHandle) == HAL_I2C_STATE_RESET) {
        I2cHandle.Instance = I2C2;
        I2cHandle.Init.ClockSpeed = I2C_DMA_SPEED;
        I2cHandle.Init.DutyCycle = I2C_DUTYCYCLE_2;
        I2cHandle.Init.OwnAddress1 = 0;
        I2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
        I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
        I2cHandle.Init.OwnAddress2 = 0;
        I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
        I2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;
        
        I2C_DMA_MspInit(&I2cHandle);
        HAL_I2C_Init(&I2cHandle);
    }
}

/**
 * @brief  De-initialize I2C peripheral and DMA
 * @retval None
 */
void I2C_DMA_DeInit(void) {
    HAL_I2C_DeInit(&I2cHandle);
}

/**
 * @brief  Write a single byte to I2C device register using DMA
 * @param  DevAddr: I2C device address (7-bit, left-shifted)
 * @param  Reg: Register address to write to
 * @param  Value: Byte value to write
 * @retval I2C_DMA_StatusTypeDef: Operation status
 */
I2C_DMA_StatusTypeDef I2C_DMA_WriteRegister(uint8_t DevAddr, uint8_t Reg, uint8_t Value) {
    HAL_StatusTypeDef status = HAL_OK;
    i2c_write_single_buf = Value;
    i2c_tx_complete = 0;

    status = HAL_I2C_Mem_Write_DMA(&I2cHandle, DevAddr, (uint16_t)Reg,
                                    I2C_MEMADD_SIZE_8BIT, &i2c_write_single_buf, 1);
    
    if (status != HAL_OK) {
        I2C_DMA_ErrorHandler();
        return I2C_DMA_ERROR;
    }

    uint32_t timeout = I2C_DMA_TIMEOUT_MAX;
    while (!i2c_tx_complete && timeout--) {}
    
    if (!i2c_tx_complete) {
        I2C_DMA_ErrorHandler();
        return I2C_DMA_TIMEOUT;
    }
    
    return I2C_DMA_OK;
}

/**
 * @brief  Read a single byte from I2C device register using DMA
 * @param  DevAddr: I2C device address (7-bit, left-shifted)
 * @param  Reg: Register address to read from
 * @param  pValue: Pointer to store the read value
 * @retval I2C_DMA_StatusTypeDef: Operation status
 */
I2C_DMA_StatusTypeDef I2C_DMA_ReadRegister(uint8_t DevAddr, uint8_t Reg, uint8_t *pValue) {
    HAL_StatusTypeDef status = HAL_OK;
    i2c_read_single_buf = 0;
    i2c_rx_complete = 0;

    status = HAL_I2C_Mem_Read_DMA(&I2cHandle, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT,
                                   &i2c_read_single_buf, 1);
    
    if (status != HAL_OK) {
        I2C_DMA_ErrorHandler();
        return I2C_DMA_ERROR;
    }

    uint32_t timeout = I2C_DMA_TIMEOUT_MAX;
    while (!i2c_rx_complete && timeout--) {}
    
    if (!i2c_rx_complete) {
        I2C_DMA_ErrorHandler();
        return I2C_DMA_TIMEOUT;
    }

    *pValue = i2c_read_single_buf;
    return I2C_DMA_OK;
}

/**
 * @brief  Read multiple bytes from I2C device register using DMA
 * @param  DevAddr: I2C device address (7-bit, left-shifted)
 * @param  Reg: Starting register address
 * @param  pBuffer: Pointer to buffer to store read data
 * @param  Length: Number of bytes to read
 * @retval I2C_DMA_StatusTypeDef: Operation status
 */
I2C_DMA_StatusTypeDef I2C_DMA_ReadBuffer(uint8_t DevAddr, uint8_t Reg, 
                                          uint8_t *pBuffer, uint16_t Length) {
    HAL_StatusTypeDef status = HAL_OK;
    i2c_rx_complete = 0;
    
    status = HAL_I2C_Mem_Read_DMA(&I2cHandle, DevAddr, (uint16_t)Reg,
                                   I2C_MEMADD_SIZE_8BIT, pBuffer, Length);
    
    if (status != HAL_OK) {
        I2C_DMA_ErrorHandler();
        return I2C_DMA_ERROR;
    }
    
    uint32_t timeout = I2C_DMA_TIMEOUT_MAX * 10;
    while (!i2c_rx_complete && timeout--) {}
    
    if (!i2c_rx_complete) {
        I2C_DMA_ErrorHandler();
        return I2C_DMA_TIMEOUT;
    }
    
    return I2C_DMA_OK;
}

/**
 * @brief  Check if I2C DMA transfer is complete
 * @retval 1 if complete, 0 if busy
 */
uint8_t I2C_DMA_IsTransferComplete(void) {
    return (i2c_tx_complete || i2c_rx_complete);
}

/**
 * @brief  I2C Memory TX Complete Callback (called from HAL)
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void I2C_DMA_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) {
        i2c_tx_complete = 1;
    }
}

/**
 * @brief  I2C Memory RX Complete Callback (called from HAL)
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void I2C_DMA_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) {
        i2c_rx_complete = 1;
    }
}

/**
 * @brief  I2C Error Callback (called from HAL)
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void I2C_DMA_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) {
        trace_printf("I2C DMA Error occurred\n");
        I2C_DMA_ErrorHandler();
    }
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Initialize I2C MSP (GPIO, DMA, NVIC)
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
static void I2C_DMA_MspInit(I2C_HandleTypeDef *hi2c) {
    GPIO_InitTypeDef gpio_init_structure;

    if (hi2c->Instance == I2C2) {
        /* Enable GPIO clock */
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* Configure I2C2 SCL (PB10) */
        gpio_init_structure.Pin = GPIO_PIN_10;
        gpio_init_structure.Mode = GPIO_MODE_AF_OD;
        gpio_init_structure.Pull = GPIO_NOPULL;
        gpio_init_structure.Speed = GPIO_SPEED_FAST;
        gpio_init_structure.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOB, &gpio_init_structure);

        /* Configure I2C2 SDA (PB11) */
        gpio_init_structure.Pin = GPIO_PIN_11;
        gpio_init_structure.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOB, &gpio_init_structure);

        /* Enable I2C2 clock */
        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_I2C2_FORCE_RESET();
        __HAL_RCC_I2C2_RELEASE_RESET();

        /* Enable DMA1 clock */
        __HAL_RCC_DMA1_CLK_ENABLE();

        /* Configure DMA for I2C2 TX (DMA1 Stream7, Channel 7) */
        hdma_i2c_tx.Instance = DMA1_Stream7;
        hdma_i2c_tx.Init.Channel = DMA_CHANNEL_7;
        hdma_i2c_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_i2c_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_i2c_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_i2c_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_i2c_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_i2c_tx.Init.Mode = DMA_NORMAL;
        hdma_i2c_tx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_i2c_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_i2c_tx);
        __HAL_LINKDMA(hi2c, hdmatx, hdma_i2c_tx);

        /* Configure DMA for I2C2 RX (DMA1 Stream2, Channel 7) */
        hdma_i2c_rx.Instance = DMA1_Stream2;
        hdma_i2c_rx.Init.Channel = DMA_CHANNEL_7;
        hdma_i2c_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_i2c_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_i2c_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_i2c_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_i2c_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_i2c_rx.Init.Mode = DMA_NORMAL;
        hdma_i2c_rx.Init.Priority = DMA_PRIORITY_HIGH;
        hdma_i2c_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        HAL_DMA_Init(&hdma_i2c_rx);
        __HAL_LINKDMA(hi2c, hdmarx, hdma_i2c_rx);

        /* Configure NVIC for DMA interrupts */
        HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 0x0E, 0x00);
        HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
        HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0x0E, 0x00);
        HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
        
        /* Configure NVIC for I2C interrupts */
        HAL_NVIC_SetPriority(I2C2_EV_IRQn, 0x0F, 0x00);
        HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_SetPriority(I2C2_ER_IRQn, 0x0F, 0x00);
        HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
    }
}

/**
 * @brief  Handle I2C DMA errors by reinitializing
 * @retval None
 */
static void I2C_DMA_ErrorHandler(void) {
    HAL_I2C_DeInit(&I2cHandle);
    I2C_DMA_Init();
}

/* HAL Callback wrappers -----------------------------------------------------*/

/**
 * @brief  HAL I2C Memory TX Complete Callback
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    I2C_DMA_MemTxCpltCallback(hi2c);
}

/**
 * @brief  HAL I2C Memory RX Complete Callback
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    I2C_DMA_MemRxCpltCallback(hi2c);
}

/**
 * @brief  HAL I2C Error Callback
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    I2C_DMA_ErrorCallback(hi2c);
}
