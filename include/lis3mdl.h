/**
 ******************************************************************************
 * @file    lis3mdl.h
 * @brief   LIS3MDL 3-axis magnetometer driver header
 * @author  ApBDC Team
 ******************************************************************************
 * @attention
 *
 * This module provides a driver for the LIS3MDL 3-axis digital magnetometer.
 * It uses I2C with DMA for efficient data transfer.
 *
 * Features:
 * - Easy initialization and configuration
 * - Blocking and non-blocking read operations
 * - Configurable full-scale range and output data rate
 * - Data ready status checking
 *
 ******************************************************************************
 */

#ifndef LIS3MDL_H
#define LIS3MDL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/

/* LIS3MDL I2C Address (SA1 pin → GND = 0x1C, SA1 pin → VCC = 0x1E) */
#define LIS3MDL_I2C_ADDRESS_LOW      0x1C  // SA1 = GND
#define LIS3MDL_I2C_ADDRESS_HIGH     0x1E  // SA1 = VCC

/* Default I2C address (assuming SA1 = GND) */
#ifndef LIS3MDL_I2C_ADDRESS
#define LIS3MDL_I2C_ADDRESS          LIS3MDL_I2C_ADDRESS_LOW
#endif

/* Device ID */
#define LIS3MDL_WHO_AM_I_VALUE       0x3D

/* Exported types ------------------------------------------------------------*/

/**
 * @brief  LIS3MDL status enumeration
 */
typedef enum {
    LIS3MDL_OK       = 0x00U,
    LIS3MDL_ERROR    = 0x01U,
    LIS3MDL_TIMEOUT  = 0x02U,
    LIS3MDL_NOT_FOUND = 0x03U
} LIS3MDL_StatusTypeDef;

/**
 * @brief  LIS3MDL full-scale range
 */
typedef enum {
    LIS3MDL_FS_4_GAUSS   = 0x00,  // ±4 gauss  (6842 LSB/gauss)
    LIS3MDL_FS_8_GAUSS   = 0x01,  // ±8 gauss  (3421 LSB/gauss)
    LIS3MDL_FS_12_GAUSS  = 0x02,  // ±12 gauss (2281 LSB/gauss)
    LIS3MDL_FS_16_GAUSS  = 0x03   // ±16 gauss (1711 LSB/gauss)
} LIS3MDL_FullScaleTypeDef;

/**
 * @brief  LIS3MDL output data rate
 */
typedef enum {
    LIS3MDL_ODR_0_625_HZ = 0x00,  // 0.625 Hz
    LIS3MDL_ODR_1_25_HZ  = 0x01,  // 1.25 Hz
    LIS3MDL_ODR_2_5_HZ   = 0x02,  // 2.5 Hz
    LIS3MDL_ODR_5_HZ     = 0x03,  // 5 Hz
    LIS3MDL_ODR_10_HZ    = 0x04,  // 10 Hz
    LIS3MDL_ODR_20_HZ    = 0x05,  // 20 Hz
    LIS3MDL_ODR_40_HZ    = 0x06,  // 40 Hz
    LIS3MDL_ODR_80_HZ    = 0x07   // 80 Hz
} LIS3MDL_ODRTypeDef;

/**
 * @brief  LIS3MDL operating mode
 */
typedef enum {
    LIS3MDL_MODE_CONTINUOUS  = 0x00,  // Continuous conversion
    LIS3MDL_MODE_SINGLE      = 0x01,  // Single conversion
    LIS3MDL_MODE_POWER_DOWN  = 0x02   // Power-down mode
} LIS3MDL_ModeTypeDef;

/**
 * @brief  LIS3MDL configuration structure
 */
typedef struct {
    LIS3MDL_FullScaleTypeDef full_scale;  // Full-scale range
    LIS3MDL_ODRTypeDef       odr;         // Output data rate
    LIS3MDL_ModeTypeDef      mode;        // Operating mode
    uint8_t                  enable_bdu;  // Block data update (1=enabled)
} LIS3MDL_ConfigTypeDef;

/**
 * @brief  LIS3MDL magnetometer data structure (in gauss)
 */
typedef struct {
    float x;  // X-axis magnetic field (gauss)
    float y;  // Y-axis magnetic field (gauss)
    float z;  // Z-axis magnetic field (gauss)
} LIS3MDL_MagDataTypeDef;

/**
 * @brief  LIS3MDL raw data structure (16-bit signed)
 */
typedef struct {
    int16_t x;  // X-axis raw value
    int16_t y;  // Y-axis raw value
    int16_t z;  // Z-axis raw value
} LIS3MDL_RawDataTypeDef;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Initialize LIS3MDL magnetometer with default configuration
 * @note   Default: ±4 gauss, 10 Hz ODR, continuous mode, BDU enabled
 * @retval LIS3MDL_StatusTypeDef: Initialization status
 */
LIS3MDL_StatusTypeDef LIS3MDL_Init(void);

/**
 * @brief  Initialize LIS3MDL magnetometer with custom configuration
 * @param  config: Pointer to configuration structure
 * @retval LIS3MDL_StatusTypeDef: Initialization status
 */
LIS3MDL_StatusTypeDef LIS3MDL_InitWithConfig(const LIS3MDL_ConfigTypeDef *config);

/**
 * @brief  De-initialize LIS3MDL magnetometer (power down)
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_DeInit(void);

/**
 * @brief  Read magnetometer data (blocking)
 * @param  data: Pointer to store magnetic field data in gauss
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_ReadMagData(LIS3MDL_MagDataTypeDef *data);

/**
 * @brief  Read raw magnetometer data (blocking)
 * @param  data: Pointer to store raw 16-bit values
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_ReadRawData(LIS3MDL_RawDataTypeDef *data);

/**
 * @brief  Check if new data is available
 * @param  data_ready: Pointer to store data ready flag (1=ready, 0=not ready)
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_IsDataReady(uint8_t *data_ready);

/**
 * @brief  Wait for data ready with timeout
 * @param  timeout_ms: Timeout in milliseconds (0 = no timeout)
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_WaitForDataReady(uint32_t timeout_ms);

/**
 * @brief  Read device ID (WHO_AM_I register)
 * @param  id: Pointer to store device ID (should be 0x3D)
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_ReadID(uint8_t *id);

/**
 * @brief  Set full-scale range
 * @param  full_scale: Full-scale range selection
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_SetFullScale(LIS3MDL_FullScaleTypeDef full_scale);

/**
 * @brief  Set output data rate
 * @param  odr: Output data rate selection
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_SetODR(LIS3MDL_ODRTypeDef odr);

/**
 * @brief  Set operating mode
 * @param  mode: Operating mode selection
 * @retval LIS3MDL_StatusTypeDef: Operation status
 */
LIS3MDL_StatusTypeDef LIS3MDL_SetMode(LIS3MDL_ModeTypeDef mode);

/**
 * @brief  Get current sensitivity (LSB/gauss) based on full-scale setting
 * @retval float: Sensitivity value
 */
float LIS3MDL_GetSensitivity(void);

#ifdef __cplusplus
}
#endif

#endif /* LIS3MDL_H */
