/**
 ******************************************************************************
 * @file    lis3mdl.c
 * @brief   LIS3MDL 3-axis magnetometer driver implementation
 * @author  ApBDC Team
 ******************************************************************************
 * @attention
 *
 * This module provides a driver for the LIS3MDL 3-axis digital magnetometer.
 * It uses I2C with DMA for efficient data transfer.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "lis3mdl.h"
#include "diag/trace.h"
#include "i2c_dma.h"

/* Private constants ---------------------------------------------------------*/

/* LIS3MDL Register Addresses */
#define LIS3MDL_REG_WHO_AM_I 0x0F // Device identification
#define LIS3MDL_REG_CTRL1 0x20    // Control register 1 (ODR, operating mode XY)
#define LIS3MDL_REG_CTRL2                                                      \
    0x21 // Control register 2 (Full-scale, reboot, soft reset)
#define LIS3MDL_REG_CTRL3 0x22   // Control register 3 (Power mode)
#define LIS3MDL_REG_CTRL4 0x23   // Control register 4 (Operating mode Z-axis)
#define LIS3MDL_REG_CTRL5 0x24   // Control register 5 (BDU, FAST_READ)
#define LIS3MDL_REG_STATUS 0x27  // Status register (data ready flags)
#define LIS3MDL_REG_OUT_X_L 0x28 // X-axis output low byte
#define LIS3MDL_REG_OUT_X_H 0x29 // X-axis output high byte
#define LIS3MDL_REG_OUT_Y_L 0x2A // Y-axis output low byte
#define LIS3MDL_REG_OUT_Y_H 0x2B // Y-axis output high byte
#define LIS3MDL_REG_OUT_Z_L 0x2C // Z-axis output low byte
#define LIS3MDL_REG_OUT_Z_H 0x2D // Z-axis output high byte

/* Number of bytes for XYZ read */
#define LIS3MDL_XYZ_REG_SIZE 6

/* Status register bits */
#define LIS3MDL_STATUS_ZYXDA 0x08 // XYZ-axis new data available

/* Sensitivity values (LSB/gauss) for different full-scale ranges */
#define LIS3MDL_SENSITIVITY_4G 6842.0f  // ±4 gauss
#define LIS3MDL_SENSITIVITY_8G 3421.0f  // ±8 gauss
#define LIS3MDL_SENSITIVITY_12G 2281.0f // ±12 gauss
#define LIS3MDL_SENSITIVITY_16G 1711.0f // ±16 gauss

/* Private variables ---------------------------------------------------------*/
static uint8_t lis3mdl_device_address = (LIS3MDL_I2C_ADDRESS << 1);
static LIS3MDL_FullScaleTypeDef current_full_scale = LIS3MDL_FS_4_GAUSS;
static float current_sensitivity = LIS3MDL_SENSITIVITY_4G;

/* Private function prototypes -----------------------------------------------*/
static LIS3MDL_StatusTypeDef LIS3MDL_WriteRegister(uint8_t reg, uint8_t value);
static LIS3MDL_StatusTypeDef LIS3MDL_ReadRegister(uint8_t reg, uint8_t *value);
static LIS3MDL_StatusTypeDef
LIS3MDL_ReadMultipleRegisters(uint8_t reg, uint8_t *buffer, uint16_t length);
static void LIS3MDL_UpdateSensitivity(void);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Initialize LIS3MDL magnetometer with default configuration
 * @retval LIS3MDL_StatusTypeDef: Initialization status
 */
LIS3MDL_StatusTypeDef LIS3MDL_Init(void) {
    LIS3MDL_ConfigTypeDef default_config = {.full_scale = LIS3MDL_FS_4_GAUSS,
                                            .odr = LIS3MDL_ODR_10_HZ,
                                            .mode = LIS3MDL_MODE_CONTINUOUS,
                                            .enable_bdu = 1};

    return LIS3MDL_InitWithConfig(&default_config);
}

/**
 * @brief  Initialize LIS3MDL magnetometer with custom configuration
 * @param  config: Pointer to configuration structure
 * @retval LIS3MDL_StatusTypeDef: Initialization status
 */
LIS3MDL_StatusTypeDef
LIS3MDL_InitWithConfig(const LIS3MDL_ConfigTypeDef *config) {
    LIS3MDL_StatusTypeDef status;
    uint8_t who_am_i = 0;

    // Initialize I2C with DMA
    I2C_DMA_Init();

    // Verify device identity
    status = LIS3MDL_ReadID(&who_am_i);
    if (status != LIS3MDL_OK) {
        trace_printf("LIS3MDL: Failed to read WHO_AM_I register\n");
        return status;
    }

    if (who_am_i != LIS3MDL_WHO_AM_I_VALUE) {
        trace_printf(
            "LIS3MDL: Device not found! WHO_AM_I=0x%02X (expected 0x3D)\n",
            who_am_i);
        return LIS3MDL_NOT_FOUND;
    }

    trace_printf("LIS3MDL: Device found (WHO_AM_I=0x%02X)\n", who_am_i);

    // Configure CTRL_REG1: Ultra-high performance XY + ODR
    uint8_t ctrl1_value = 0x60 | ((config->odr & 0x07) << 2);
    status = LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL1, ctrl1_value);
    if (status != LIS3MDL_OK)
        return status;

    // Configure CTRL_REG2: Full-scale selection
    uint8_t ctrl2_value = (config->full_scale & 0x03) << 5;
    status = LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL2, ctrl2_value);
    if (status != LIS3MDL_OK)
        return status;

    // Store current full-scale and update sensitivity
    current_full_scale = config->full_scale;
    LIS3MDL_UpdateSensitivity();

    // Configure CTRL_REG3: Operating mode
    uint8_t ctrl3_value = (config->mode == LIS3MDL_MODE_CONTINUOUS) ? 0x00
                          : (config->mode == LIS3MDL_MODE_SINGLE)   ? 0x01
                                                                    : 0x03;
    status = LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL3, ctrl3_value);
    if (status != LIS3MDL_OK)
        return status;

    // Configure CTRL_REG4: Ultra-high performance Z-axis
    status = LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL4, 0x0C);
    if (status != LIS3MDL_OK)
        return status;

    // Configure CTRL_REG5: Block data update
    uint8_t ctrl5_value = config->enable_bdu ? 0x40 : 0x00;
    status = LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL5, ctrl5_value);
    if (status != LIS3MDL_OK)
        return status;

    trace_printf("LIS3MDL: Initialization complete\n");
    return LIS3MDL_OK;
}

/**
 * @brief  De-initialize LIS3MDL magnetometer (power down)
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_DeInit(void) {
    // Set power-down mode in CTRL_REG3
    return LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL3, 0x03);
}

/**
 * @brief  Read magnetometer data (blocking)
 * @param  data: Pointer to store magnetic field data in gauss
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_ReadMagData(LIS3MDL_MagDataTypeDef *data) {
    LIS3MDL_RawDataTypeDef raw_data;
    LIS3MDL_StatusTypeDef status;

    // Read raw data
    status = LIS3MDL_ReadRawData(&raw_data);
    if (status != LIS3MDL_OK) {
        return status;
    }

    // Convert to gauss using current sensitivity
    data->x = (float)raw_data.x / current_sensitivity;
    data->y = (float)raw_data.y / current_sensitivity;
    data->z = (float)raw_data.z / current_sensitivity;

    return LIS3MDL_OK;
}

/**
 * @brief  Read raw magnetometer data (blocking)
 * @param  data: Pointer to store raw 16-bit values
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_ReadRawData(LIS3MDL_RawDataTypeDef *data) {
    uint8_t raw_buffer[LIS3MDL_XYZ_REG_SIZE];
    LIS3MDL_StatusTypeDef status;

    // Read all 6 output bytes starting from OUT_X_L
    status = LIS3MDL_ReadMultipleRegisters(LIS3MDL_REG_OUT_X_L, raw_buffer,
                                           LIS3MDL_XYZ_REG_SIZE);
    if (status != LIS3MDL_OK) {
        return status;
    }

    // Reconstruct signed 16-bit values (little-endian)
    data->x = (int16_t)(raw_buffer[1] << 8 | raw_buffer[0]);
    data->y = (int16_t)(raw_buffer[3] << 8 | raw_buffer[2]);
    data->z = (int16_t)(raw_buffer[5] << 8 | raw_buffer[4]);

    return LIS3MDL_OK;
}

/**
 * @brief  Check if new data is available
 * @param  data_ready: Pointer to store data ready flag (1=ready, 0=not ready)
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_IsDataReady(uint8_t *data_ready) {
    uint8_t status_reg = 0;
    LIS3MDL_StatusTypeDef status;

    status = LIS3MDL_ReadRegister(LIS3MDL_REG_STATUS, &status_reg);
    if (status != LIS3MDL_OK) {
        return status;
    }

    *data_ready = (status_reg & LIS3MDL_STATUS_ZYXDA) ? 1 : 0;
    return LIS3MDL_OK;
}

/**
 * @brief  Wait for data ready with timeout
 * @param  timeout_ms: Timeout in milliseconds (0 = no timeout)
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_WaitForDataReady(uint32_t timeout_ms) {
    uint8_t data_ready = 0;
    uint32_t timeout_counter = timeout_ms * 100; // Approximate loop iterations

    while (!data_ready && (timeout_ms == 0 || timeout_counter > 0)) {
        LIS3MDL_StatusTypeDef status = LIS3MDL_IsDataReady(&data_ready);
        if (status != LIS3MDL_OK) {
            return status;
        }

        if (timeout_ms > 0) {
            timeout_counter--;
        }
    }

    if (!data_ready && timeout_ms > 0) {
        return LIS3MDL_TIMEOUT;
    }

    return LIS3MDL_OK;
}

/**
 * @brief  Read device ID (WHO_AM_I register)
 * @param  id: Pointer to store device ID (should be 0x3D)
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_ReadID(uint8_t *id) {
    return LIS3MDL_ReadRegister(LIS3MDL_REG_WHO_AM_I, id);
}

/**
 * @brief  Set full-scale range
 * @param  full_scale: Full-scale range selection
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef
LIS3MDL_SetFullScale(LIS3MDL_FullScaleTypeDef full_scale) {
    uint8_t ctrl2_value = (full_scale & 0x03) << 5;
    LIS3MDL_StatusTypeDef status;

    status = LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL2, ctrl2_value);
    if (status == LIS3MDL_OK) {
        current_full_scale = full_scale;
        LIS3MDL_UpdateSensitivity();
    }

    return status;
}

/**
 * @brief  Set output data rate
 * @param  odr: Output data rate selection
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_SetODR(LIS3MDL_ODRTypeDef odr) {
    uint8_t ctrl1_value = 0x60 | ((odr & 0x07) << 2);
    return LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL1, ctrl1_value);
}

/**
 * @brief  Set operating mode
 * @param  mode: Operating mode selection
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_SetMode(LIS3MDL_ModeTypeDef mode) {
    uint8_t ctrl3_value = (mode == LIS3MDL_MODE_CONTINUOUS) ? 0x00
                          : (mode == LIS3MDL_MODE_SINGLE)   ? 0x01
                                                            : 0x03;
    return LIS3MDL_WriteRegister(LIS3MDL_REG_CTRL3, ctrl3_value);
}

/**
 * @brief  Get current sensitivity (LSB/gauss) based on full-scale setting
 * @retval float: Sensitivity value
 */
float LIS3MDL_GetSensitivity(void) {
    return current_sensitivity;
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Write a single byte to LIS3MDL register
 * @param  reg: Register address
 * @param  value: Value to write
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
static LIS3MDL_StatusTypeDef LIS3MDL_WriteRegister(uint8_t reg, uint8_t value) {
    I2C_DMA_StatusTypeDef i2c_status;

    i2c_status = I2C_DMA_WriteRegister(lis3mdl_device_address, reg, value);

    switch (i2c_status) {
    case I2C_DMA_OK:
        return LIS3MDL_OK;
    case I2C_DMA_TIMEOUT:
        return LIS3MDL_TIMEOUT;
    default:
        return LIS3MDL_ERROR;
    }
}

/**
 * @brief  Read a single byte from LIS3MDL register
 * @param  reg: Register address
 * @param  value: Pointer to store read value
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
static LIS3MDL_StatusTypeDef LIS3MDL_ReadRegister(uint8_t reg, uint8_t *value) {
    I2C_DMA_StatusTypeDef i2c_status;

    i2c_status = I2C_DMA_ReadRegister(lis3mdl_device_address, reg, value);

    switch (i2c_status) {
    case I2C_DMA_OK:
        return LIS3MDL_OK;
    case I2C_DMA_TIMEOUT:
        return LIS3MDL_TIMEOUT;
    default:
        return LIS3MDL_ERROR;
    }
}

/**
 * @brief  Read multiple bytes from LIS3MDL registers
 * @param  reg: Starting register address
 * @param  buffer: Pointer to buffer to store read data
 * @param  length: Number of bytes to read
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
static LIS3MDL_StatusTypeDef
LIS3MDL_ReadMultipleRegisters(uint8_t reg, uint8_t *buffer, uint16_t length) {
    I2C_DMA_StatusTypeDef i2c_status;

    i2c_status =
        I2C_DMA_ReadBuffer(lis3mdl_device_address, reg, buffer, length);

    switch (i2c_status) {
    case I2C_DMA_OK:
        return LIS3MDL_OK;
    case I2C_DMA_TIMEOUT:
        return LIS3MDL_TIMEOUT;
    default:
        return LIS3MDL_ERROR;
    }
}

/**
 * @brief  Update sensitivity based on current full-scale setting
 * @retval None
 */
static void LIS3MDL_UpdateSensitivity(void) {
    switch (current_full_scale) {
    case LIS3MDL_FS_4_GAUSS:
        current_sensitivity = LIS3MDL_SENSITIVITY_4G;
        break;
    case LIS3MDL_FS_8_GAUSS:
        current_sensitivity = LIS3MDL_SENSITIVITY_8G;
        break;
    case LIS3MDL_FS_12_GAUSS:
        current_sensitivity = LIS3MDL_SENSITIVITY_12G;
        break;
    case LIS3MDL_FS_16_GAUSS:
        current_sensitivity = LIS3MDL_SENSITIVITY_16G;
        break;
    default:
        current_sensitivity = LIS3MDL_SENSITIVITY_4G;
        break;
    }
}
