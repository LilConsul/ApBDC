#include "diag/trace.h"
#include "es_wifi_io.h"
#include "sensor/magnetometer_stream.h"
#include "stm32f4xx_hal.h"
#include "web_content.h"
#include <stdio.h>
#include <stm32f413h_discovery.h>
#include <string.h>
#include <wifi.h>
#include "error_handler.hpp"
#include "wifi_conf.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

/* Server state machine states */
typedef enum {
    SERVER_STATE_IDLE = 0,
    SERVER_STATE_SENT_RESPONSE
} ServerState_t;

/* Private variables ---------------------------------------------------------*/
static uint8_t http_buffer[HTTP_BUFFER_SIZE];
static uint8_t ip_addr[4];
static ServerState_t server_state = SERVER_STATE_IDLE;
static uint32_t request_count = 0;

/* Interrupt flags for main loop */
volatile uint8_t http_process_flag = 0;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void LED_Init(void);
static void WiFi_InitAndConnect(void);
static void HTTP_ServerProcess(void);
static float simple_atan2(float y, float x);

[[noreturn]]
int main(int argc, char *argv[]) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    Error_Handler_Init();

    trace_printf("\n\n");
    trace_printf("================================\n");
    trace_printf("  STM32F413H WiFi HTTP Server\n");
    trace_printf("================================\n");
    trace_printf("Initializing...\n\n");

    /* Initialize magnetometer streaming module */
    trace_printf("Step 1: Initializing magnetometer...\n");
    if (Magnetometer_Init() != 0) {
        trace_printf("ERROR: Magnetometer initialization failed!\n");
        Error_Handler("Magnetometer Init Failed");
    }
    trace_printf("  -> Magnetometer initialized\n\n");

    WiFi_InitAndConnect();

    trace_printf("\nStep 5: Starting magnetometer sampling...\n");
    Magnetometer_StartSampling();
    trace_printf("  -> Magnetometer sampling started at 20Hz\n");

    trace_printf("\n===================\n");
    trace_printf("  Server Ready!\n");
    trace_printf("===================\n");
    trace_printf("Access the server at: http://%d.%d.%d.%d\n", ip_addr[0],
                 ip_addr[1], ip_addr[2], ip_addr[3]);
    trace_printf("Waiting for HTTP requests...\n");
    trace_printf("CPU will enter low-power mode (WFI) between requests\n\n");

    BSP_LED_On(LED_GREEN);

    while (1) {
        if (http_process_flag) {
            http_process_flag = 0;
            HTTP_ServerProcess();
        }

        __WFI();
    }
}

static void WiFi_InitAndConnect(void) {
    WIFI_Status_t status;

    trace_printf("Step 2: Initializing WiFi module...\n");
    status = WIFI_Init();
    if (status != WIFI_STATUS_OK) {
        Error_Handler("ERROR: WiFi initialization failed! (Status: %d)\n",
                      status);
    }
    trace_printf("  -> WiFi module initialized\n");

    trace_printf("\nStep 3: Connecting to network '%s'...\n", WIFI_SSID);
    status = WIFI_Connect(WIFI_SSID, WIFI_PASSWORD, WIFI_ECN_WPA2_PSK);
    if (status != WIFI_STATUS_OK) {
        Error_Handler("ERROR: WiFi connection failed! (Status: %d)\nCheck SSID "
                      "and password in include/wifi/wifi_conf.hpp",
                      status);
    }
    trace_printf("  -> Connected to WiFi network\n");

    trace_printf("\nStep 4: Getting IP address...\n");
    status = WIFI_GetIP_Address(ip_addr);
    if (status != WIFI_STATUS_OK) {
        Error_Handler("ERROR: Failed to get IP address! (Status: %d)\n",
                      status);
    }
    trace_printf("  -> IP Address: %d.%d.%d.%d\n", ip_addr[0], ip_addr[1],
                 ip_addr[2], ip_addr[3]);

    trace_printf("\nStep 4a: Starting HTTP server on port %d...\n",
                 HTTP_SERVER_PORT);
    status = WIFI_StartServer(0, WIFI_TCP_PROTOCOL, "HTTP", HTTP_SERVER_PORT);
    if (status != WIFI_STATUS_OK) {
        Error_Handler("ERROR: Failed to start HTTP server! (Status: %d)\n",
                      status);
    }
    trace_printf("  -> HTTP server started\n");
}

/**
 * @brief  Process HTTP requests
 * @param  None
 * @retval None
 */
static void HTTP_ServerProcess(void) {
    uint16_t recv_len = 0;
    uint16_t sent_len = 0;
    WIFI_Status_t status;

    if (server_state != SERVER_STATE_IDLE) {
        return; /* Server busy :( */
    }

    status = WIFI_ReceiveData(0, http_buffer, HTTP_BUFFER_SIZE - 1, &recv_len);

    if (status != WIFI_STATUS_OK || recv_len == 0) {
        return;
    }

    if (recv_len > 0) {
        http_buffer[recv_len] = '\0';

        if (strstr((char *)http_buffer, "[SOMA]") != NULL ||
            strstr((char *)http_buffer, "[EOMA]") != NULL ||
            strstr((char *)http_buffer, "Unhandled") != NULL ||
            strstr((char *)http_buffer, "Socket") != NULL ||
            strcmp((char *)http_buffer, "-1") == 0 ||
            (recv_len <= 3 && http_buffer[0] == '-')) {
            return; /* Silently ignore status messages */
        }

        request_count++;

        char *request_line = strtok((char *)http_buffer, "\r\n");
        trace_printf("[%lu] %s\n", request_count,
                     request_line ? request_line : "Invalid request");

        BSP_LED_Off(LED_GREEN);

        http_buffer[recv_len] = '\0';

        if (strncmp((char *)http_buffer, "GET", 3) == 0) {
            /* Get latest magnetometer sample */
            MagSample_t sample;
            char x_str[16] = "-.----";
            char y_str[16] = "-.----";
            char z_str[16] = "-.----";
            char heading_str[16] = "---";
            uint32_t timestamp = 0;
            float heading = 0.0f;

            if (Magnetometer_GetAvailableSamples() > 0 &&
                Magnetometer_ReadSample(&sample)) {
                snprintf(x_str, sizeof(x_str), "%.4f", sample.x);
                snprintf(y_str, sizeof(y_str), "%.4f", sample.y);
                snprintf(z_str, sizeof(z_str), "%.4f", sample.z);
                timestamp = sample.timestamp_ms;

                /* Calculate heading from X and Y components */
                /* simple_atan2(y, x) gives angle in radians, convert to degrees
                 */
                heading =
                    simple_atan2(sample.y, sample.x) * 180.0f / 3.14159265f;

                /* Normalize to 0-360 range */
                if (heading < 0) {
                    heading += 360.0f;
                }

                snprintf(heading_str, sizeof(heading_str), "%.1f", heading);
            }

            char response[2048];
            int len = snprintf(response, sizeof(response), HTTP_HTML_HEADER);
            len += snprintf(response + len, sizeof(response) - len,
                           HTML_PAGE_TEMPLATE, heading_str, heading_str, x_str,
                           y_str, z_str, timestamp);

            status = WIFI_SendData(0, (uint8_t *)response, len, &sent_len);

            if (status == WIFI_STATUS_OK) {
                trace_printf("[%lu] Sent HTML page with data (%d bytes)\n",
                             request_count, sent_len);
            } else {
                trace_printf("[%lu] ERROR: Failed to send page\n",
                             request_count);
            }

            BSP_LED_On(LED_GREEN);

            /* Close connection and restart server to cleanup sockets */
            trace_printf("[%lu] Closing connection and restarting server...\n",
                         request_count);
            WIFI_CloseClientConnection();
            WIFI_StopServer(0);
            HAL_Delay(100);

            status = WIFI_StartServer(0, WIFI_TCP_PROTOCOL, "HTTP",
                                      HTTP_SERVER_PORT);
            if (status == WIFI_STATUS_OK) {
                trace_printf("[%lu] Server restarted successfully\n",
                             request_count);

            } else {
                Error_Handler(
                    "[%lu] ERROR: Failed to restart server (Status: %d)\n",
                    request_count, status);
            }

            if (status != WIFI_STATUS_OK) {
                trace_printf("[%lu] ERROR: All restart attempts failed, "
                             "reinitializing WiFi...\n",
                             request_count);
                BSP_LED_On(LED_RED);
                WIFI_ResetModule();
                HAL_Delay(500);
                WiFi_InitAndConnect();
                BSP_LED_Off(LED_RED);
            }
        }

        server_state = SERVER_STATE_IDLE;
    }
}

static void LED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    BSP_LED_Init(LED_GREEN);
    BSP_LED_Init(LED_RED);
    BSP_LED_Off(LED_GREEN);
    BSP_LED_Off(LED_RED);

    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET);
}

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {.ClockType = 0,
                                            .SYSCLKSource = 0,
                                            .AHBCLKDivider = 0,
                                            .APB1CLKDivider = 0,
                                            .APB2CLKDivider = 0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler("Clock Config Failed - HSI");
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // 16 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;  // 16 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  // 16 MHz

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler("Clock Config Failed - Buses");
    }

    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* Simple atan2 approximation for heading calculation */
static float simple_atan2(float y, float x) {
    const float PI = 3.14159265f;
    float abs_y = (y < 0) ? -y : y;
    float angle;

    if (x >= 0) {
        float r = (x - abs_y) / (x + abs_y);
        angle = 0.785398163f - 0.785398163f * r; // PI/4
    } else {
        float r = (x + abs_y) / (abs_y - x);
        angle = 2.356194490f - 0.785398163f * r; // 3*PI/4
    }

    return (y < 0) ? -angle : angle;
}

#pragma GCC diagnostic pop
