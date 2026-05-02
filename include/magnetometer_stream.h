/**
 ******************************************************************************
 * @file    magnetometer_stream.h
 * @brief   Interrupt-driven magnetometer streaming module header
 * @author  ApBDC Team
 ******************************************************************************
 * @attention
 *
 * This module provides interrupt-driven magnetometer data acquisition and
 * streaming capabilities. It uses TIM2 for periodic sampling and I2C DMA
 * for non-blocking sensor communication.
 *
 * Features:
 * - 100% interrupt-driven (no blocking operations)
 * - Circular buffer for continuous data storage
 * - Configurable sample rate (default 20Hz)
 * - Thread-safe buffer access
 * - DMA-based I2C communication
 *
 ******************************************************************************
 */

#ifndef MAGNETOMETER_STREAM_H
#define MAGNETOMETER_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "lis3mdl.h"
#include <stdint.h>

/* Configuration -------------------------------------------------------------*/
#define MAG_SAMPLE_RATE_HZ 20  /* Sampling rate in Hz */
#define MAG_BUFFER_SIZE 32     /* Buffer size (1.6 seconds at 20Hz) */
#define MAG_TIMER_PERIOD_MS 50 /* Timer period: 1000/20 = 50ms */

/* Exported types ------------------------------------------------------------*/

/**
 * @brief  Magnetometer sample with timestamp
 */
typedef struct {
    uint32_t timestamp_ms; /* Timestamp in milliseconds */
    float x;               /* X-axis magnetic field (gauss) */
    float y;               /* Y-axis magnetic field (gauss) */
    float z;               /* Z-axis magnetic field (gauss) */
} MagSample_t;

/**
 * @brief  Circular buffer for magnetometer data
 */
typedef struct {
    MagSample_t samples[MAG_BUFFER_SIZE]; /* Sample array */
    volatile uint16_t write_index;        /* Write position */
    volatile uint16_t read_index;         /* Read position */
    volatile uint16_t count;              /* Number of samples in buffer */
} MagBuffer_t;

/**
 * @brief  Magnetometer streaming statistics
 */
typedef struct {
    uint32_t total_samples;    /* Total samples acquired */
    uint32_t buffer_overruns;  /* Number of buffer overruns */
    uint32_t i2c_errors;       /* Number of I2C errors */
    uint32_t last_sample_time; /* Timestamp of last sample */
} MagStats_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Initialize magnetometer streaming module
 * @note   Initializes LIS3MDL sensor and configures TIM2 for periodic sampling
 * @retval 0 on success, non-zero on error
 */
uint8_t Magnetometer_Init(void);

/**
 * @brief  Start magnetometer sampling
 * @note   Enables TIM2 interrupt to begin periodic sampling
 * @retval None
 */
void Magnetometer_StartSampling(void);

/**
 * @brief  Stop magnetometer sampling
 * @note   Disables TIM2 interrupt to stop sampling
 * @retval None
 */
void Magnetometer_StopSampling(void);

/**
 * @brief  Get number of available samples in buffer
 * @retval Number of samples available to read
 */
uint16_t Magnetometer_GetAvailableSamples(void);

/**
 * @brief  Read one sample from buffer (non-blocking)
 * @param  sample: Pointer to store the sample data
 * @retval 1 if sample was read, 0 if buffer is empty
 */
uint8_t Magnetometer_ReadSample(MagSample_t *sample);

/**
 * @brief  Get streaming statistics
 * @param  stats: Pointer to store statistics
 * @retval None
 */
void Magnetometer_GetStats(MagStats_t *stats);

/**
 * @brief  Reset streaming statistics
 * @retval None
 */
void Magnetometer_ResetStats(void);

/**
 * @brief  Timer callback - called from TIM2 ISR
 * @note   Initiates non-blocking I2C DMA read
 * @retval None
 */
void Magnetometer_TimerCallback(void);

/**
 * @brief  DMA complete callback - called from I2C DMA complete ISR
 * @note   Stores acquired data in circular buffer
 * @retval None
 */
void Magnetometer_DMACompleteCallback(void);

/**
 * @brief  Error callback - called from I2C error ISR
 * @note   Logs error and prepares for retry on next timer tick
 * @retval None
 */
void Magnetometer_ErrorCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* MAGNETOMETER_STREAM_H */
