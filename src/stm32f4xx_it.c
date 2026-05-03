/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @brief   Interrupt Service Routines.
 ******************************************************************************
 */

#include "sensor/i2c_dma.h"
#include "sensor/magnetometer_stream.h"
#include "stm32f4xx_hal.h"

/* External flag declarations from main.cpp */
extern volatile uint8_t http_process_flag;

/* External TIM2 handle from magnetometer_stream.c */
extern TIM_HandleTypeDef htim2;

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/

/**
 * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void) {
    while (1) {
    }
}

/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void) {
    while (1) {
    }
}

/**
 * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void) {
    while (1) {
    }
}

/**
 * @brief This function handles Pre-fetch fault, memory access fault.
 */
void BusFault_Handler(void) {
    while (1) {
    }
}

/**
 * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void) {
    while (1) {
    }
}

/**
 * @brief This function handles System service call via SWI instruction.
 */
void SVC_Handler(void) {
}

/**
 * @brief This function handles Debug monitor.
 */
void DebugMon_Handler(void) {
}

/**
 * @brief This function handles Pendable request for system service.
 */
void PendSV_Handler(void) {
}

/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void) {
    static uint32_t poll_counter = 0;
    
    HAL_IncTick();
    
    /* Poll for HTTP data every 100ms as fallback (in case DRDY interrupt is missed) */
    poll_counter++;
    if (poll_counter >= 100) {
        poll_counter = 0;
        http_process_flag = 1;
    }
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/******************************************************************************/

/**
 * @brief This function handles DMA1 stream2 global interrupt (I2C2 RX).
 */
void DMA1_Stream2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_i2c_rx);
}

/**
 * @brief This function handles DMA1 stream7 global interrupt (I2C2 TX).
 */
void DMA1_Stream7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_i2c_tx);
}

/**
 * @brief This function handles I2C2 event interrupt.
 */
void I2C2_EV_IRQHandler(void) {
    HAL_I2C_EV_IRQHandler(&I2cHandle);
}

/**
 * @brief This function handles I2C2 error interrupt.
 */
void I2C2_ER_IRQHandler(void) {
    HAL_I2C_ER_IRQHandler(&I2cHandle);
}

/**
 * @brief This function handles EXTI line0 interrupt (User Button on PA0).
 */
void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/**
 * @brief This function handles EXTI line[15:10] interrupts (WiFi DRDY on PG12).
 */
void EXTI15_10_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
}

/**
 * @brief This function handles TIM2 global interrupt (Magnetometer sampling).
 */
void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim2);
}

/**
 * @brief TIM2 Period Elapsed Callback - Initiates magnetometer DMA read
 * @param htim: TIM handle pointer
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        Magnetometer_TimerCallback(); /* Non-blocking, starts I2C DMA */
    }
}

/**
 * @brief TIMEx Break Callback (weak implementation)
 * @param htim: TIM handle pointer
 */
__attribute__((weak)) void HAL_TIMEx_BreakCallback(TIM_HandleTypeDef *htim) {
    /* Prevent unused argument(s) compilation warning */
    (void)htim;
    /* NOTE: This function should not be modified, when the callback is needed,
             the HAL_TIMEx_BreakCallback could be implemented in the user file
     */
}

/**
 * @brief TIMEx Commutation Callback (weak implementation)
 * @param htim: TIM handle pointer
 */
__attribute__((weak)) void HAL_TIMEx_CommutCallback(TIM_HandleTypeDef *htim) {
    /* Prevent unused argument(s) compilation warning */
    (void)htim;
    /* NOTE: This function should not be modified, when the callback is needed,
             the HAL_TIMEx_CommutCallback could be implemented in the user file
     */
}