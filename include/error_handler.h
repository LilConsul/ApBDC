#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error handler with LED indication and trace output.
 *
 * @param fmt  printf-style format string describing the error.
 *             Must not be NULL.
 * @param ...   Additional arguments matching format specifiers in fmt.
 *
 * @note This function does not return. It disables interrupts,
 *       signals error via LEDs, outputs diagnostic information,
 *       and enters an infinite loop.
 */
void Error_Handler(const char *fmt, ...);

/**
 * @brief Error handler with file and line information
 * @param error_message: String describing the error
 * @param file: Source file name where error occurred
 * @param line: Line number where error occurred
 * @note This function does not return - enters infinite loop
 */
void Error_Handler_Debug(const char *error_message, const char *file,
                         uint32_t line);

/**
 * @brief Initialize error handler (LED GPIO configuration)
 * @note Called automatically by Error_Handler if not initialized
 */
void Error_Handler_Init(void);

/* Convenience macro for debug builds */
#ifdef DEBUG
#define ERROR_HANDLER(msg) Error_Handler_Debug(msg, __FILE__, __LINE__)
#else
#define ERROR_HANDLER(msg) Error_Handler(msg)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ERROR_HANDLER_H */
