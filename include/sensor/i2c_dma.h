/**
 ******************************************************************************
 * @file    i2c_dma.h
 * @brief   I2C with DMA support driver header
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

#ifndef I2C_DMA_H
#define I2C_DMA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/
#ifndef I2C_DMA_SPEED
#define I2C_DMA_SPEED 100000 // 100 kHz standard mode
#endif

#define I2C_DMA_TIMEOUT_MAX 0x3000 // Timeout for blocking operations

/* Exported types ------------------------------------------------------------*/
typedef enum {
    I2C_DMA_OK = 0x00U,
    I2C_DMA_ERROR = 0x01U,
    I2C_DMA_BUSY = 0x02U,
    I2C_DMA_TIMEOUT = 0x03U
} I2C_DMA_StatusTypeDef;

/* Exported variables --------------------------------------------------------*/
extern I2C_HandleTypeDef I2cHandle;
extern DMA_HandleTypeDef hdma_i2c_tx;
extern DMA_HandleTypeDef hdma_i2c_rx;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Initialize I2C peripheral with DMA support
 * @note   Configures I2C2 on PB10 (SCL) and PB11 (SDA)
 *         DMA1 Stream7 for TX, DMA1 Stream2 for RX
 * @retval None
 */
void I2C_DMA_Init(void);

/**
 * @brief  De-initialize I2C peripheral and DMA
 * @retval None
 */
void I2C_DMA_DeInit(void);

/**
 * @brief  Write a single byte to I2C device register using DMA
 * @param  DevAddr: I2C device address (7-bit, left-shifted)
 * @param  Reg: Register address to write to
 * @param  Value: Byte value to write
 * @retval I2C_DMA_StatusTypeDef: Operation status
 */
I2C_DMA_StatusTypeDef I2C_DMA_WriteRegister(uint8_t DevAddr, uint8_t Reg,
                                            uint8_t Value);

/**
 * @brief  Read a single byte from I2C device register using DMA
 * @param  DevAddr: I2C device address (7-bit, left-shifted)
 * @param  Reg: Register address to read from
 * @param  pValue: Pointer to store the read value
 * @retval I2C_DMA_StatusTypeDef: Operation status
 */
I2C_DMA_StatusTypeDef I2C_DMA_ReadRegister(uint8_t DevAddr, uint8_t Reg,
                                           uint8_t *pValue);

/**
 * @brief  Read multiple bytes from I2C device register using DMA
 * @param  DevAddr: I2C device address (7-bit, left-shifted)
 * @param  Reg: Starting register address
 * @param  pBuffer: Pointer to buffer to store read data
 * @param  Length: Number of bytes to read
 * @retval I2C_DMA_StatusTypeDef: Operation status
 */
I2C_DMA_StatusTypeDef I2C_DMA_ReadBuffer(uint8_t DevAddr, uint8_t Reg,
                                         uint8_t *pBuffer, uint16_t Length);

/**
 * @brief  Check if I2C DMA transfer is complete
 * @retval 1 if complete, 0 if busy
 */
uint8_t I2C_DMA_IsTransferComplete(void);

/**
 * @brief  I2C Memory TX Complete Callback (called from HAL)
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void I2C_DMA_MemTxCpltCallback(I2C_HandleTypeDef *hi2c);

/**
 * @brief  I2C Memory RX Complete Callback (called from HAL)
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void I2C_DMA_MemRxCpltCallback(I2C_HandleTypeDef *hi2c);

/**
 * @brief  I2C Error Callback (called from HAL)
 * @param  hi2c: I2C handle pointer
 * @retval None
 */
void I2C_DMA_ErrorCallback(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif

#endif /* I2C_DMA_H */
