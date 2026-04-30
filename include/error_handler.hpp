#ifndef ERROR_HANDLER_HPP
#define ERROR_HANDLER_HPP

#include "stm32f4xx_hal.h"
#include <cstdint>

/**
 * @brief Error handler with LED indication and trace output
 * @param error_message: String describing the error
 * @note This function does not return - enters infinite loop
 */
void Error_Handler(const char* error_message);

/**
 * @brief Error handler with file and line information
 * @param error_message: String describing the error
 * @param file: Source file name where error occurred
 * @param line: Line number where error occurred
 * @note This function does not return - enters infinite loop
 */
void Error_Handler_Debug(const char* error_message, const char* file, uint32_t line);

/**
 * @brief Initialize error handler (LED GPIO configuration)
 * @note Called automatically by Error_Handler if not initialized
 */
void Error_Handler_Init();

// Convenience macro for debug builds
#ifdef DEBUG
    #define ERROR_HANDLER(msg) Error_Handler_Debug(msg, __FILE__, __LINE__)
#else
    #define ERROR_HANDLER(msg) Error_Handler(msg)
#endif

#endif /* ERROR_HANDLER_HPP */

// Made with Bob
