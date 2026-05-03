#include <stm32f413h_discovery.h>
#include "error_handler.hpp"
#include "diag/trace.h"
#include <cstring>

// Static flag to track initialization
static bool error_handler_initialized = false;

/**
 * @brief Initialize error handler (uses BSP LEDs)
 */
void Error_Handler_Init() {
    if (error_handler_initialized)
        return;

    // LEDs are already initialized by BSP_LED_Init() in main
    // No additional initialization needed
    error_handler_initialized = true;
}

/**
 * @brief Basic error handler
 */
void Error_Handler(const char *error_message) {
    // Disable interrupts to prevent further issues
    __disable_irq();

    // Turn on red LED and turn off green LED
    BSP_LED_On(LED_RED);
    BSP_LED_Off(LED_GREEN);

    // Output error message
    trace_printf("\n");
    trace_printf("=====================================\n");
    trace_printf("[ERROR] System Error Occurred\n");
    trace_printf("=====================================\n");
    if (error_message != nullptr) {
        trace_printf("Message: %s\n", error_message);
    }
    trace_printf("System Tick: %lu\n", HAL_GetTick());
    trace_printf("=====================================\n");
    trace_printf("System halted. Red LED is ON.\n");
    trace_printf("Please reset the board.\n");
    trace_printf("=====================================\n");

    // Enter infinite loop
    while (true) {
        // Blink red LED slowly to indicate error state
        HAL_Delay(500);
        BSP_LED_Toggle(LED_RED);
    }
}

/**
 * @brief Debug error handler with file and line info
 */
void Error_Handler_Debug(const char *error_message, const char *file,
                         uint32_t line) {
    // Disable interrupts
    __disable_irq();

    // Turn on red LED and turn off green LED
    BSP_LED_On(LED_RED);
    BSP_LED_Off(LED_GREEN);

    // Output detailed error message
    trace_printf("\n");
    trace_printf("=====================================\n");
    trace_printf("[ERROR] System Error Occurred\n");
    trace_printf("=====================================\n");
    if (error_message != nullptr) {
        trace_printf("Message: %s\n", error_message);
    }
    if (file != nullptr) {
        trace_printf("File: %s\n", file);
    }
    trace_printf("Line: %lu\n", line);
    trace_printf("System Tick: %lu\n", HAL_GetTick());
    trace_printf("=====================================\n");
    trace_printf("System halted. Red LED is ON.\n");
    trace_printf("Please reset the board.\n");
    trace_printf("=====================================\n");

    // Enter infinite loop with blinking red LED
    while (true) {
        HAL_Delay(500);
        BSP_LED_Toggle(LED_RED);
    }
}
