#include "error_handler.hpp"
#include "diag/trace.h"
#include <cstring>

// LED Configuration for STM32F413H-Discovery
#define ERROR_LED_PIN           GPIO_PIN_3
#define ERROR_LED_GPIO_PORT     GPIOE
#define ERROR_LED_GPIO_CLK()    __HAL_RCC_GPIOE_CLK_ENABLE()

// Status LED (optional - can be used for heartbeat before error)
#define STATUS_LED_PIN          GPIO_PIN_2
#define STATUS_LED_GPIO_PORT    GPIOE

// Static flag to track initialization
static bool error_handler_initialized = false;

/**
 * @brief Initialize error handler GPIO
 */
void Error_Handler_Init()
{
    if (error_handler_initialized)
        return;
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Enable GPIO clock
    ERROR_LED_GPIO_CLK();
    
    // Configure red LED pin (PE3)
    GPIO_InitStruct.Pin = ERROR_LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ERROR_LED_GPIO_PORT, &GPIO_InitStruct);
    
    // Ensure LED is off initially
    HAL_GPIO_WritePin(ERROR_LED_GPIO_PORT, ERROR_LED_PIN, GPIO_PIN_RESET);
    
    // Configure green LED pin (PE2) - optional
    GPIO_InitStruct.Pin = STATUS_LED_PIN;
    HAL_GPIO_Init(STATUS_LED_GPIO_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(STATUS_LED_GPIO_PORT, STATUS_LED_PIN, GPIO_PIN_RESET);
    
    error_handler_initialized = true;
}

/**
 * @brief Basic error handler
 */
void Error_Handler(const char* error_message)
{
    // Disable interrupts to prevent further issues
    __disable_irq();
    
    // Initialize LED if not already done
    if (!error_handler_initialized)
    {
        Error_Handler_Init();
    }
    
    // Turn on red LED
    HAL_GPIO_WritePin(ERROR_LED_GPIO_PORT, ERROR_LED_PIN, GPIO_PIN_SET);
    
    // Turn off status LED if it was on
    HAL_GPIO_WritePin(STATUS_LED_GPIO_PORT, STATUS_LED_PIN, GPIO_PIN_RESET);
    
    // Output error message
    trace_printf("\n");
    trace_printf("=====================================\n");
    trace_printf("[ERROR] System Error Occurred\n");
    trace_printf("=====================================\n");
    if (error_message != nullptr)
    {
        trace_printf("Message: %s\n", error_message);
    }
    trace_printf("System Tick: %lu\n", HAL_GetTick());
    trace_printf("=====================================\n");
    trace_printf("System halted. Red LED is ON.\n");
    trace_printf("Please reset the board.\n");
    trace_printf("=====================================\n");
    
    // Enter infinite loop
    while (true)
    {
        // Blink LED slowly to indicate error state
        HAL_Delay(500);
        HAL_GPIO_TogglePin(ERROR_LED_GPIO_PORT, ERROR_LED_PIN);
    }
}

/**
 * @brief Debug error handler with file and line info
 */
void Error_Handler_Debug(const char* error_message, const char* file, uint32_t line)
{
    // Disable interrupts
    __disable_irq();
    
    // Initialize LED if not already done
    if (!error_handler_initialized)
    {
        Error_Handler_Init();
    }
    
    // Turn on red LED
    HAL_GPIO_WritePin(ERROR_LED_GPIO_PORT, ERROR_LED_PIN, GPIO_PIN_SET);
    
    // Turn off status LED
    HAL_GPIO_WritePin(STATUS_LED_GPIO_PORT, STATUS_LED_PIN, GPIO_PIN_RESET);
    
    // Output detailed error message
    trace_printf("\n");
    trace_printf("=====================================\n");
    trace_printf("[ERROR] System Error Occurred\n");
    trace_printf("=====================================\n");
    if (error_message != nullptr)
    {
        trace_printf("Message: %s\n", error_message);
    }
    if (file != nullptr)
    {
        trace_printf("File: %s\n", file);
    }
    trace_printf("Line: %lu\n", line);
    trace_printf("System Tick: %lu\n", HAL_GetTick());
    trace_printf("=====================================\n");
    trace_printf("System halted. Red LED is ON.\n");
    trace_printf("Please reset the board.\n");
    trace_printf("=====================================\n");
    
    // Enter infinite loop with blinking LED
    while (true)
    {
        HAL_Delay(500);
        HAL_GPIO_TogglePin(ERROR_LED_GPIO_PORT, ERROR_LED_PIN);
    }
}

// Made with Bob
