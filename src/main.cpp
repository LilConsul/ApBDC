/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"

#include "stm32f4xx_hal.h"
#include "stm32f413h_discovery.h"

/* Exported functions ------------------------------------------------------- */
static void I2Cx_MspInit(I2C_HandleTypeDef *hi2c);
static void I2Cx_Init(void);
static void I2Cx_Error(void);
void I2Cx_WriteData(uint8_t Addr, uint8_t Reg, uint8_t Value);
uint8_t I2Cx_ReadData(uint8_t Addr, uint8_t Reg);
uint8_t I2Cx_ReadBuffer(uint8_t Addr, uint8_t Reg, uint8_t *pBuffer, uint16_t Length);
double bmp280_compensate_T_double(int32_t adc_T);

// ----------------------------------------------------------------------------
#ifndef BSP_I2C_SPEED
#define BSP_I2C_SPEED                          100000
#endif /* BSP_I2C_SPEED */
#define I2Cx_TIMEOUT_MAX                    0x3000 /*<! The value of the maximal timeout for I2C waiting loops */

// Definitions specific for BMP280
#define BMP280_I2C_ADDRESS           0x76     // BMP280 address with SD0 connected to GND

#define BMP280_REG_TEMP_MSB   0xFA
#define BMP280_REG_TEMP_LSB   0xFB
#define BMP280_REG_TEMP_xLSB  0xFC

#define BMP280_REG_CTRL_MEAS  0xF4
#define BMP280_REG_CONFIG     0xF5

#define BMP280_REG_ID         0xD0

// Number for bytes to read
#define BMP280_TEMP_REG_SIZE  3

// Number of bytes to read
#define TMP102_TEMP_REG_SIZE  1

#define dig_T1_R	0x88
#define dig_T2_R	0x8A
#define dig_T3_R	0x8C

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef I2cHandle;
DMA_HandleTypeDef hdma_i2c_tx;
DMA_HandleTypeDef hdma_i2c_rx;

// Флаги для відстеження завершення операцій DMA
volatile uint8_t i2c_tx_complete = 0;
volatile uint8_t i2c_rx_complete = 0;

uint16_t dig_T1;
uint16_t dig_T2;
uint16_t dig_T3;

/* Main ----------------------------------------------------------------------*/
/**
 * @brief  Main program
 * @param  None
 * @retval None
 */
int main(void) {
	// Data buffer
	uint8_t aRxBuffer[BMP280_TEMP_REG_SIZE];

	// I2C address must be shifted one bit left. Last bit reflects R/W (0/1)
	uint8_t DevAddr = BMP280_I2C_ADDRESS << 1;

	uint32_t temp_raw;
	// LED configuration
	BSP_LED_Init(LED3);
	//BSP_LED_InBUser button configuration
	BSP_PB_Init(BUTTON_WAKEUP, BUTTON_MODE_GPIO);

	// I2C2 config - SDA=D14(PB11) SCL=D15(PB10)
	I2Cx_Init();

	// Configuration specific for BMP280
	uint8_t osrs_t = 1;             //Temperature oversampling x 1
	uint8_t osrs_p = 1;             //Pressure oversampling x 1

	uint8_t mode = 3;               //Normal mode
	uint8_t t_sb = 5;               //Tstandby 1000ms
	uint8_t filter = 0;             //Filter off
	uint8_t spi3w_en = 0;           //3-wire SPI Disable

	uint8_t ctrl_meas_reg = (osrs_t << 5) | (osrs_p << 2) | mode;
	uint8_t config_reg = (t_sb << 5) | (filter << 2) | spi3w_en;

	uint8_t id = 0;
	id = I2Cx_ReadData(DevAddr, BMP280_REG_ID);
	trace_printf("Device ID: %d\n", id);

	I2Cx_WriteData(DevAddr, BMP280_REG_CTRL_MEAS, ctrl_meas_reg);
	I2Cx_WriteData(DevAddr, BMP280_REG_CONFIG, config_reg);

	// Calibration data
	I2Cx_ReadBuffer(DevAddr, dig_T1_R, (uint8_t *)&dig_T1, sizeof(dig_T1));
	I2Cx_ReadBuffer(DevAddr, dig_T2_R, (uint8_t *)&dig_T2, sizeof(dig_T2));
	I2Cx_ReadBuffer(DevAddr, dig_T3_R, (uint8_t *)&dig_T3, sizeof(dig_T3));

	trace_printf("Calibration data: dig_T1=%d, dig_T2=%d, dig_T3=%d\n", dig_T1, dig_T2, dig_T3);

	// Program main loop
	while (1) {
		BSP_LED_Off(LED3);
		// Wait for a user button
		while (BSP_PB_GetState(BUTTON_WAKEUP) != 1) {
		}
		while (BSP_PB_GetState(BUTTON_WAKEUP) != 0) {
		}

		// Read BMP280 temp registers (3 bytes) TEMP
		if (I2Cx_ReadBuffer(DevAddr, BMP280_REG_TEMP_MSB, aRxBuffer,
		BMP280_TEMP_REG_SIZE) != 0) {
			trace_printf("I2C Error occurred.\n");
			// Turn on LED4 to signal error
			BSP_LED_On(LED4);
			while (1) {
			}
		}

		BSP_LED_On(LED3);
		// For BMP280
		temp_raw = (aRxBuffer[0] << 12) | (aRxBuffer[1] << 4)
				| (aRxBuffer[2] >> 4);
		trace_printf("Temp regs read: %d %d %d\n", aRxBuffer[0], aRxBuffer[1],
				aRxBuffer[2]);
		trace_printf("Temp raw: %d\n", temp_raw);

		double temperature = 0.0;
		temperature = bmp280_compensate_T_double(temp_raw);
		trace_printf("temperature: %lf\n", temperature);
	}
}

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
static void I2Cx_MspInit(I2C_HandleTypeDef *hi2c) {
	GPIO_InitTypeDef gpio_init_structure;

	if (hi2c->Instance == I2C2) {
		/*** Configure the GPIOs ***/
		/* Enable GPIO clock */
		__HAL_RCC_GPIOB_CLK_ENABLE();

		/* Configure I2C2 SCL as alternate function */
		gpio_init_structure.Pin = GPIO_PIN_10;
		gpio_init_structure.Mode = GPIO_MODE_AF_OD;
		gpio_init_structure.Pull = GPIO_NOPULL;
		gpio_init_structure.Speed = GPIO_SPEED_FAST;
		gpio_init_structure.Alternate = GPIO_AF4_I2C2;
		HAL_GPIO_Init(GPIOB, &gpio_init_structure);

		/* Configure I2C SDA as alternate function */
		gpio_init_structure.Pin = GPIO_PIN_11;
		gpio_init_structure.Alternate = GPIO_AF4_I2C2;
		HAL_GPIO_Init(GPIOB, &gpio_init_structure);

		/*** Configure the I2C peripheral ***/
		/* Enable I2C clock */
		__HAL_RCC_I2C2_CLK_ENABLE();

		/* Force the I2C peripheral clock reset */
		__HAL_RCC_I2C2_FORCE_RESET();

		/* Release the I2C peripheral clock reset */
		__HAL_RCC_I2C2_RELEASE_RESET();

		/*** Configure DMA ***/
		/* Enable DMA1 clock */
		__HAL_RCC_DMA1_CLK_ENABLE();

		/* Configure DMA for I2C2 TX (DMA1_Stream7, Channel 7) */
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

		/* Configure DMA for I2C2 RX (DMA1_Stream2, Channel 7) */
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

		/* NVIC configuration for DMA */
		HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 0x0E, 0x00);
		HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);

		HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0x0E, 0x00);
		HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);

		/* Enable and set I2Cx Interrupt to a lower priority */
		HAL_NVIC_SetPriority(I2C2_EV_IRQn, 0x0F, 0x00);
		HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);

		/* Enable and set I2Cx Interrupt to a lower priority */
		HAL_NVIC_SetPriority(I2C2_ER_IRQn, 0x0F, 0x00);
		HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
	}
}

/**
 * @brief  I2Cx Bus initialization.
 */
static void I2Cx_Init(void) {
	if (HAL_I2C_GetState(&I2cHandle) == HAL_I2C_STATE_RESET) {
		I2cHandle.Instance = I2C2;
		I2cHandle.Init.ClockSpeed = BSP_I2C_SPEED;
		I2cHandle.Init.DutyCycle = I2C_DUTYCYCLE_2;
		I2cHandle.Init.OwnAddress1 = 0;
		I2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
		I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
		I2cHandle.Init.OwnAddress2 = 0;
		I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
		I2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;

		/* Init the I2C */
		I2Cx_MspInit(&I2cHandle);
		HAL_I2C_Init(&I2cHandle);
	}
}

/**
  * @brief  I2Cx error treatment function
  */
static void I2Cx_Error(void) {
  /* De-initialize the SPI communication BUS */
  HAL_I2C_DeInit(&I2cHandle);

  /* Re-Initialize the SPI communication BUS */
  I2Cx_Init();
}

/**
  * @brief  Writes a value in a register of the device through BUS using DMA.
  * @param  Addr: Device address on BUS Bus.
  * @param  Reg: The target register address to write
  * @param  Value: The target register value to be written
  */
void I2Cx_WriteData(uint8_t Addr, uint8_t Reg, uint8_t Value) {
	HAL_StatusTypeDef status = HAL_OK;

	i2c_tx_complete = 0;
	status = HAL_I2C_Mem_Write_DMA(&I2cHandle, Addr, (uint16_t)Reg,
									I2C_MEMADD_SIZE_8BIT, &Value, 1);

	if (status != HAL_OK) {
		I2Cx_Error();
		return;
	}

	// Очікування завершення передачі
	uint32_t timeout = I2Cx_TIMEOUT_MAX;
	while (!i2c_tx_complete && timeout--) {
		// Можна додати HAL_Delay(1) для економії CPU
	}

	if (!i2c_tx_complete) {
		I2Cx_Error();
	}
}

/**
  * @brief  Reads a register of the device through BUS using DMA.
  * @param  Addr: Device address on BUS Bus.
  * @param  Reg: The target register address to write
  * @retval Data read at register address
  */
uint8_t I2Cx_ReadData(uint8_t Addr, uint8_t Reg) {
	HAL_StatusTypeDef status = HAL_OK;
	uint8_t value = 0;

	i2c_rx_complete = 0;
	status = HAL_I2C_Mem_Read_DMA(&I2cHandle, Addr, Reg, I2C_MEMADD_SIZE_8BIT,
								   &value, 1);

	if (status != HAL_OK) {
		I2Cx_Error();
		return 0;
	}

	// Очікування завершення прийому
	uint32_t timeout = I2Cx_TIMEOUT_MAX;
	while (!i2c_rx_complete && timeout--) {
		// Можна додати HAL_Delay(1)
	}

	if (!i2c_rx_complete) {
		I2Cx_Error();
	}

	return value;
}

/**
 * @brief  Reads multiple data on the BUS using DMA.
 * @param  Addr: I2C Address
 * @param  Reg: Reg Address
 * @param  pBuffer: pointer to read data buffer
 * @param  Length: length of the data
 * @retval 0 if no problems to read multiple data
 */
uint8_t I2Cx_ReadBuffer(uint8_t Addr, uint8_t Reg, uint8_t *pBuffer, uint16_t Length) {
	HAL_StatusTypeDef status = HAL_OK;

	i2c_rx_complete = 0;
	status = HAL_I2C_Mem_Read_DMA(&I2cHandle, Addr, (uint16_t)Reg,
								   I2C_MEMADD_SIZE_8BIT, pBuffer, Length);

	if (status != HAL_OK) {
		I2Cx_Error();
		return 1;
	}

	// Очікування завершення прийому
	uint32_t timeout = I2Cx_TIMEOUT_MAX * 10; // Більший timeout для буфера
	while (!i2c_rx_complete && timeout--) {
		// Можна додати HAL_Delay(1)
	}

	if (!i2c_rx_complete) {
		I2Cx_Error();
		return 1;
	}

	return 0;
}

/* HAL I2C callbacks */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
	if (hi2c->Instance == I2C2) {
		i2c_tx_complete = 1;
	}
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
	if (hi2c->Instance == I2C2) {
		i2c_rx_complete = 1;
	}
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
	if (hi2c->Instance == I2C2) {
		trace_printf("I2C DMA Error occurred\n");
		I2Cx_Error();
	}
}

double bmp280_compensate_T_double(int32_t adc_T) {
	double var1, var2, T;
	var1 = (((double) adc_T) / 16384.0 - ((double) dig_T1) / 1024.0)
			* ((double) dig_T2);
	var2 = ((((double) adc_T) / 131072.0 - ((double) dig_T1) / 8192.0)
			* (((double) adc_T) / 131072.0 - ((double) dig_T1) / 8192.0))
			* ((double) dig_T3);
	T = (var1 + var2) / 5120.0;
	return T;
}

/* DMA interrupt handlers for I2C2 */
void DMA1_Stream7_IRQHandler(void) {
	HAL_DMA_IRQHandler(&hdma_i2c_tx);
}

void DMA1_Stream2_IRQHandler(void) {
	HAL_DMA_IRQHandler(&hdma_i2c_rx);
}
