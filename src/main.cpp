/*
 * LIS3MDL magnetometer with button interrupt
 */

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

// ----------------------------------------------------------------------------
#ifndef BSP_I2C_SPEED
#define BSP_I2C_SPEED                            100000
#endif
#define I2Cx_TIMEOUT_MAX                         0x3000

// ---- Button definitions ---------------------------------------------------
#define BUTTON_PIN                               GPIO_PIN_0
#define BUTTON_GPIO_PORT                         GPIOA
#define BUTTON_GPIO_CLK_ENABLE()                 __HAL_RCC_GPIOA_CLK_ENABLE()
#define BUTTON_EXTI_IRQn                         EXTI0_IRQn

// ---- LIS3MDL definitions --------------------------------------------------
// SA1 pin → GND = 0x1C, SA1 pin → VCC = 0x1E
#define LIS3MDL_I2C_ADDRESS      0x1C

// Register addresses
#define LIS3MDL_REG_WHO_AM_I     0x0F   // Should read 0x3D
#define LIS3MDL_REG_CTRL1        0x20   // ODR, operating mode XY
#define LIS3MDL_REG_CTRL2        0x21   // Full-scale selection, reboot, soft reset
#define LIS3MDL_REG_CTRL3        0x22   // Power mode (continuous = 0x00)
#define LIS3MDL_REG_CTRL4        0x23   // Operating mode Z-axis
#define LIS3MDL_REG_CTRL5        0x24   // BDU
#define LIS3MDL_REG_STATUS       0x27   // Data ready flags
#define LIS3MDL_REG_OUT_X_L      0x28   // X low  (auto-increments to 0x2D)
#define LIS3MDL_REG_OUT_X_H      0x29
#define LIS3MDL_REG_OUT_Y_L      0x2A
#define LIS3MDL_REG_OUT_Y_H      0x2B
#define LIS3MDL_REG_OUT_Z_L      0x2C
#define LIS3MDL_REG_OUT_Z_H      0x2D

// Number of bytes for a full XYZ read (6 bytes = 3 axes × 2 bytes each)
#define LIS3MDL_XYZ_REG_SIZE     6

/*
 * Sensitivity (LSB/gauss) depending on full-scale setting in CTRL_REG2:
 *   ±4  gauss → 6842 LSB/gauss
 *   ±8  gauss → 3421 LSB/gauss
 *   ±12 gauss → 2281 LSB/gauss
 *   ±16 gauss → 1711 LSB/gauss
 */
#define LIS3MDL_SENSITIVITY      6842.0f   // matching FS = ±4 gauss below

/* Private variables --------------------------------------------------------*/
I2C_HandleTypeDef I2cHandle;
DMA_HandleTypeDef hdma_i2c_tx;
DMA_HandleTypeDef hdma_i2c_rx;

volatile uint8_t i2c_tx_complete = 0;
volatile uint8_t i2c_rx_complete = 0;
volatile uint8_t button_pressed_flag = 0;

/* Function prototypes ------------------------------------------------------*/
static void Button_GPIO_Init(void);
static void LIS3MDL_Init(void);
static void LIS3MDL_ReadMagnetometer(void);

/* Main ---------------------------------------------------------------------*/
int main(void) {
    HAL_Init();
    
    BSP_LED_Init(LED3);
    
    Button_GPIO_Init();
    I2Cx_Init();
    LIS3MDL_Init();

    trace_printf("LIS3MDL configured. Press button to read magnetometer.\n");

    // Main loop - check flag set by interrupt
    while (1) {
        if (button_pressed_flag) {
            button_pressed_flag = 0;
            LIS3MDL_ReadMagnetometer();
        }
        // CPU can enter low-power mode here if needed
        __WFI();  // Wait For Interrupt
    }
}

/**
 * @brief  Configure button GPIO as external interrupt
 * @retval None
 */
static void Button_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Enable GPIO clock
    BUTTON_GPIO_CLK_ENABLE();
    
    // Configure GPIO pin as interrupt on rising edge
    GPIO_InitStruct.Pin = BUTTON_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BUTTON_GPIO_PORT, &GPIO_InitStruct);
    
    // Set EXTI interrupt priority and enable it
    HAL_NVIC_SetPriority(BUTTON_EXTI_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(BUTTON_EXTI_IRQn);
}

/**
 * @brief  Initialize LIS3MDL magnetometer
 * @retval None
 */
static void LIS3MDL_Init(void) {
    uint8_t DevAddr = LIS3MDL_I2C_ADDRESS << 1;
    
    // Verify device identity
    uint8_t who = I2Cx_ReadData(DevAddr, LIS3MDL_REG_WHO_AM_I);
    trace_printf("WHO_AM_I: 0x%02X (expected 0x3D)\n", who);
    if (who != 0x3D) {
        trace_printf("LIS3MDL not found! Check wiring and SA1 pin.\n");
        BSP_LED_On(LED4);
        while (1) {}
    }

    // Configure LIS3MDL
    // CTRL_REG1: Ultra-high performance XY + 10 Hz ODR
    I2Cx_WriteData(DevAddr, LIS3MDL_REG_CTRL1, 0x60);
    
    // CTRL_REG2: ±4 gauss full scale
    I2Cx_WriteData(DevAddr, LIS3MDL_REG_CTRL2, 0x00);
    
    // CTRL_REG3: Continuous-conversion mode
    I2Cx_WriteData(DevAddr, LIS3MDL_REG_CTRL3, 0x00);
    
    // CTRL_REG4: Ultra-high performance Z
    I2Cx_WriteData(DevAddr, LIS3MDL_REG_CTRL4, 0x0C);
    
    // CTRL_REG5: Block data update enabled
    I2Cx_WriteData(DevAddr, LIS3MDL_REG_CTRL5, 0x40);
}

/**
 * @brief  Read magnetometer data and display
 * @retval None
 */
static void LIS3MDL_ReadMagnetometer(void) {
    uint8_t rawBuffer[LIS3MDL_XYZ_REG_SIZE];
    uint8_t DevAddr = LIS3MDL_I2C_ADDRESS << 1;
    
    trace_printf("Starting magnetometer read...\n");
    BSP_LED_On(LED3);
    
    // Wait for data ready (optional)
    uint8_t status = 0;
    uint32_t drdy_timeout = 10000;
    while (!(status & 0x08) && drdy_timeout--) {
        status = I2Cx_ReadData(DevAddr, LIS3MDL_REG_STATUS);
    }
    
    if (drdy_timeout == 0) {
        trace_printf("Data ready timeout! Status: 0x%02X\n", status);
    }
    
    // Read all 6 output bytes starting from OUT_X_L
    trace_printf("Reading I2C buffer...\n");
    uint8_t result = I2Cx_ReadBuffer(DevAddr, LIS3MDL_REG_OUT_X_L, rawBuffer, LIS3MDL_XYZ_REG_SIZE);
    trace_printf("I2C read result: %d\n", result);
    
    if (result != 0) {
        trace_printf("I2C Error occurred.\n");
        BSP_LED_On(LED4);
        return;
    }
    
    // Reconstruct signed 16-bit values (little-endian)
    int16_t raw_x = (int16_t)(rawBuffer[1] << 8 | rawBuffer[0]);
    int16_t raw_y = (int16_t)(rawBuffer[3] << 8 | rawBuffer[2]);
    int16_t raw_z = (int16_t)(rawBuffer[5] << 8 | rawBuffer[4]);
    
    trace_printf("Raw  X=%d  Y=%d  Z=%d\n", raw_x, raw_y, raw_z);
    
    // Convert to gauss
    float mag_x = raw_x / LIS3MDL_SENSITIVITY;
    float mag_y = raw_y / LIS3MDL_SENSITIVITY;
    float mag_z = raw_z / LIS3MDL_SENSITIVITY;
    
    trace_printf("Mag  X=%.4f  Y=%.4f  Z=%.4f  (gauss)\n", mag_x, mag_y, mag_z);
    
    BSP_LED_Off(LED3);
    trace_printf("Magnetometer read complete.\n");
}

/**
 * @brief  GPIO EXTI callback - called when button interrupt occurs
 * @param  GPIO_Pin: Pin number that triggered the interrupt
 * @retval None
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == BUTTON_PIN) {
        trace_puts("Button pressed\n");
        button_pressed_flag = 1;
    }
}

/* --------------------------------------------------------------------------*/
/* I2C / DMA infrastructure                                                  */
/* --------------------------------------------------------------------------*/

static void I2Cx_MspInit(I2C_HandleTypeDef *hi2c) {
    GPIO_InitTypeDef gpio_init_structure;

    if (hi2c->Instance == I2C2) {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio_init_structure.Pin = GPIO_PIN_10;
        gpio_init_structure.Mode = GPIO_MODE_AF_OD;
        gpio_init_structure.Pull = GPIO_NOPULL;
        gpio_init_structure.Speed = GPIO_SPEED_FAST;
        gpio_init_structure.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOB, &gpio_init_structure);

        gpio_init_structure.Pin = GPIO_PIN_11;
        gpio_init_structure.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOB, &gpio_init_structure);

        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_I2C2_FORCE_RESET();
        __HAL_RCC_I2C2_RELEASE_RESET();

        __HAL_RCC_DMA1_CLK_ENABLE();

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

        HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 0x0E, 0x00);
        HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
        HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0x0E, 0x00);
        HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
        HAL_NVIC_SetPriority(I2C2_EV_IRQn, 0x0F, 0x00);
        HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_SetPriority(I2C2_ER_IRQn, 0x0F, 0x00);
        HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
    }
}

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
        I2Cx_MspInit(&I2cHandle);
        HAL_I2C_Init(&I2cHandle);
    }
}

static void I2Cx_Error(void) {
    HAL_I2C_DeInit(&I2cHandle);
    I2Cx_Init();
}

static uint8_t i2c_write_single_buf = 0;

void I2Cx_WriteData(uint8_t Addr, uint8_t Reg, uint8_t Value) {
    HAL_StatusTypeDef status = HAL_OK;
    i2c_write_single_buf = Value;
    i2c_tx_complete = 0;

    status = HAL_I2C_Mem_Write_DMA(&I2cHandle, Addr, (uint16_t)Reg,
                                    I2C_MEMADD_SIZE_8BIT, &i2c_write_single_buf, 1);
    if (status != HAL_OK) { I2Cx_Error(); return; }

    uint32_t timeout = I2Cx_TIMEOUT_MAX;
    while (!i2c_tx_complete && timeout--) {}
    if (!i2c_tx_complete) I2Cx_Error();
}

static uint8_t i2c_read_single_buf = 0;

uint8_t I2Cx_ReadData(uint8_t Addr, uint8_t Reg) {
    HAL_StatusTypeDef status = HAL_OK;
    i2c_read_single_buf = 0;
    i2c_rx_complete = 0;

    status = HAL_I2C_Mem_Read_DMA(&I2cHandle, Addr, Reg, I2C_MEMADD_SIZE_8BIT,
                                   &i2c_read_single_buf, 1);
    if (status != HAL_OK) { I2Cx_Error(); return 0; }

    uint32_t timeout = I2Cx_TIMEOUT_MAX;
    while (!i2c_rx_complete && timeout--) {}
    if (!i2c_rx_complete) { I2Cx_Error(); return 0; }

    return i2c_read_single_buf;
}

uint8_t I2Cx_ReadBuffer(uint8_t Addr, uint8_t Reg, uint8_t *pBuffer, uint16_t Length) {
    HAL_StatusTypeDef status = HAL_OK;
    i2c_rx_complete = 0;
    status = HAL_I2C_Mem_Read_DMA(&I2cHandle, Addr, (uint16_t)Reg,
                                   I2C_MEMADD_SIZE_8BIT, pBuffer, Length);
    if (status != HAL_OK) { I2Cx_Error(); return 1; }
    uint32_t timeout = I2Cx_TIMEOUT_MAX * 10;
    while (!i2c_rx_complete && timeout--) {}
    if (!i2c_rx_complete) { I2Cx_Error(); return 1; }
    return 0;
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) i2c_tx_complete = 1;
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) i2c_rx_complete = 1;
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C2) {
        trace_printf("I2C DMA Error occurred\n");
        I2Cx_Error();
    }
}