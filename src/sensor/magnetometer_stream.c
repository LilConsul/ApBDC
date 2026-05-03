/**
 ******************************************************************************
 * @file    magnetometer_stream.c
 * @brief   Interrupt-driven magnetometer streaming module implementation
 * @author  ApBDC Team
 ******************************************************************************
 * @attention
 *
 * This module provides 100% interrupt-driven magnetometer data acquisition.
 * No blocking operations - all I2C communication uses DMA.
 *
 * Architecture:
 * 1. TIM2 interrupt (50ms) → Initiates I2C DMA read
 * 2. I2C DMA complete interrupt → Stores data in circular buffer
 * 3. Main loop reads from buffer when needed (no waiting)
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "sensor/magnetometer_stream.h"
#include "diag/trace.h"
#include "sensor/i2c_dma.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define TIM2_PRESCALER (16000 - 1)            /* 16 MHz / 16000 = 1 kHz */
#define TIM2_PERIOD (MAG_TIMER_PERIOD_MS - 1) /* 50ms period for 20Hz */

/* Private variables ---------------------------------------------------------*/
static MagBuffer_t mag_buffer = {0};
static MagStats_t mag_stats = {0};
TIM_HandleTypeDef htim2 = {
    0}; /* Non-static so it can be accessed from stm32f4xx_it.c */
static volatile uint8_t sampling_active = 0;
static volatile uint8_t dma_in_progress = 0;

/* Temporary buffer for DMA read (raw sensor data) */
static uint8_t dma_rx_buffer[6] = {0};

/* External I2C handle from i2c_dma.c */
extern I2C_HandleTypeDef I2cHandle;

/* Private function prototypes -----------------------------------------------*/
static void TIM2_Init(void);
static void Buffer_Write(const MagSample_t *sample);
static uint8_t Buffer_Read(MagSample_t *sample);

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Initialize magnetometer streaming module
 * @retval 0 on success, non-zero on error
 */
uint8_t Magnetometer_Init(void) {
    LIS3MDL_StatusTypeDef status;

    trace_printf("Magnetometer: Initializing streaming module...\n");

    /* Initialize LIS3MDL with 20Hz ODR to match our sampling rate */
    LIS3MDL_ConfigTypeDef config = {.full_scale = LIS3MDL_FS_4_GAUSS,
                                    .odr = LIS3MDL_ODR_20_HZ,
                                    .mode = LIS3MDL_MODE_CONTINUOUS,
                                    .enable_bdu = 1};

    status = LIS3MDL_InitWithConfig(&config);
    if (status != LIS3MDL_OK) {
        trace_printf("Magnetometer: ERROR - LIS3MDL initialization failed!\n");
        return 1;
    }

    trace_printf("Magnetometer: LIS3MDL initialized (20Hz, ±4 gauss)\n");

    /* Initialize TIM2 for periodic sampling */
    TIM2_Init();
    trace_printf("Magnetometer: TIM2 configured for %dHz sampling\n",
                 MAG_SAMPLE_RATE_HZ);

    /* Reset buffer and statistics */
    memset(&mag_buffer, 0, sizeof(mag_buffer));
    memset(&mag_stats, 0, sizeof(mag_stats));

    trace_printf("Magnetometer: Initialization complete\n");
    return 0;
}

/**
 * @brief  Start magnetometer sampling
 */
void Magnetometer_StartSampling(void) {
    if (!sampling_active) {
        sampling_active = 1;
        dma_in_progress = 0;

        /* Start TIM2 */
        HAL_TIM_Base_Start_IT(&htim2);

        trace_printf("Magnetometer: Sampling started\n");
    }
}

/**
 * @brief  Stop magnetometer sampling
 */
void Magnetometer_StopSampling(void) {
    if (sampling_active) {
        sampling_active = 0;

        /* Stop TIM2 */
        HAL_TIM_Base_Stop_IT(&htim2);

        trace_printf("Magnetometer: Sampling stopped\n");
    }
}

/**
 * @brief  Get number of available samples in buffer
 */
uint16_t Magnetometer_GetAvailableSamples(void) {
    return mag_buffer.count;
}

/**
 * @brief  Read one sample from buffer (non-blocking)
 */
uint8_t Magnetometer_ReadSample(MagSample_t *sample) {
    return Buffer_Read(sample);
}

/**
 * @brief  Get streaming statistics
 */
void Magnetometer_GetStats(MagStats_t *stats) {
    if (stats != NULL) {
        /* Disable interrupts briefly for atomic read */
        __disable_irq();
        memcpy(stats, &mag_stats, sizeof(MagStats_t));
        __enable_irq();
    }
}

/**
 * @brief  Reset streaming statistics
 */
void Magnetometer_ResetStats(void) {
    __disable_irq();
    memset(&mag_stats, 0, sizeof(mag_stats));
    __enable_irq();
}

/**
 * @brief  Timer callback - called from TIM2 ISR
 * @note   Initiates non-blocking I2C DMA read
 */
void Magnetometer_TimerCallback(void) {
    HAL_StatusTypeDef hal_status;

    /* Skip if sampling is not active or DMA is busy */
    if (!sampling_active || dma_in_progress) {
        return;
    }

    /* Mark DMA as in progress */
    dma_in_progress = 1;

    /* Initiate non-blocking DMA read of magnetometer data (6 bytes: X, Y, Z) */
    /* LIS3MDL register 0x28 (OUT_X_L) with auto-increment reads all 6 bytes */
    hal_status =
        HAL_I2C_Mem_Read_DMA(&I2cHandle, (LIS3MDL_I2C_ADDRESS << 1),
                             0x28 | 0x80, /* 0x80 enables auto-increment */
                             I2C_MEMADD_SIZE_8BIT, dma_rx_buffer, 6);

    if (hal_status != HAL_OK) {
        /* DMA initiation failed - clear flag and increment error counter */
        dma_in_progress = 0;
        mag_stats.i2c_errors++;
    }
}

/**
 * @brief  DMA complete callback - called from I2C DMA complete ISR
 * @note   Stores acquired data in circular buffer
 */
void Magnetometer_DMACompleteCallback(void) {
    MagSample_t sample;
    int16_t raw_x, raw_y, raw_z;
    float sensitivity;

    /* Clear DMA in progress flag */
    dma_in_progress = 0;

    /* Parse raw data (little-endian) */
    raw_x = (int16_t)((dma_rx_buffer[1] << 8) | dma_rx_buffer[0]);
    raw_y = (int16_t)((dma_rx_buffer[3] << 8) | dma_rx_buffer[2]);
    raw_z = (int16_t)((dma_rx_buffer[5] << 8) | dma_rx_buffer[4]);

    /* Convert to gauss (sensitivity for ±4 gauss range) */
    sensitivity = LIS3MDL_GetSensitivity();
    sample.x = (float)raw_x / sensitivity;
    sample.y = (float)raw_y / sensitivity;
    sample.z = (float)raw_z / sensitivity;
    sample.timestamp_ms = HAL_GetTick();

    /* Store in circular buffer */
    Buffer_Write(&sample);

    /* Update statistics */
    mag_stats.total_samples++;
    mag_stats.last_sample_time = sample.timestamp_ms;
}

/**
 * @brief  Error callback - called from I2C error ISR
 */
void Magnetometer_ErrorCallback(void) {
    /* Clear DMA in progress flag */
    dma_in_progress = 0;

    /* Increment error counter */
    mag_stats.i2c_errors++;

    /* Error will be retried on next timer tick */
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Initialize TIM2 for periodic sampling
 */
static void TIM2_Init(void) {
    /* Enable TIM2 clock */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Configure TIM2 */
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = TIM2_PRESCALER;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = TIM2_PERIOD;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
        trace_printf("Magnetometer: ERROR - TIM2 initialization failed!\n");
        return;
    }

    /* Configure TIM2 interrupt */
    HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0); /* Lower priority than I2C DMA */
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

/**
 * @brief  Write sample to circular buffer
 * @param  sample: Pointer to sample data
 */
static void Buffer_Write(const MagSample_t *sample) {
    /* Disable interrupts for atomic operation */
    __disable_irq();

    /* Check for buffer overflow */
    if (mag_buffer.count >= MAG_BUFFER_SIZE) {
        /* Buffer full - overwrite oldest sample */
        mag_buffer.read_index = (mag_buffer.read_index + 1) % MAG_BUFFER_SIZE;
        mag_stats.buffer_overruns++;
    } else {
        mag_buffer.count++;
    }

    /* Write sample */
    mag_buffer.samples[mag_buffer.write_index] = *sample;
    mag_buffer.write_index = (mag_buffer.write_index + 1) % MAG_BUFFER_SIZE;

    __enable_irq();
}

/**
 * @brief  Read sample from circular buffer
 * @param  sample: Pointer to store sample data
 * @retval 1 if sample was read, 0 if buffer is empty
 */
static uint8_t Buffer_Read(MagSample_t *sample) {
    uint8_t result = 0;

    /* Disable interrupts for atomic operation */
    __disable_irq();

    if (mag_buffer.count > 0) {
        /* Read sample */
        *sample = mag_buffer.samples[mag_buffer.read_index];
        mag_buffer.read_index = (mag_buffer.read_index + 1) % MAG_BUFFER_SIZE;
        mag_buffer.count--;
        result = 1;
    }

    __enable_irq();

    return result;
}
