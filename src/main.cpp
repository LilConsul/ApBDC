#include "diag/trace.h"
#include "stm32f413h_discovery.h"
#include "stm32f4xx_hal.h"
#include "wifi.h"
#include <stdio.h>
#include <string.h>
#include "error_handler.hpp"
#include "wifi_conf.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

/* Private defines -----------------------------------------------------------*/
#define HTTP_RESPONSE_HEADER                                                   \
    "HTTP/1.1 200 OK\r\n"                                                      \
    "Content-Type: text/html\r\n"                                              \
    "Connection: close\r\n"                                                    \
    "\r\n"

#define HTML_PAGE                                                              \
    "<!DOCTYPE html>"                                                          \
    "<html><head><title>STM32F413H WiFi Server</title>"                        \
    "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;}"           \
    "h1{color:#0066cc;}p{font-size:18px;}</style></head>"                      \
    "<body><h1>STM32F413H Discovery WiFi Server</h1>"                          \
    "<p>WiFi module is working!</p>"                                           \
    "<p>Board: STM32F413H-Discovery</p>"                                       \
    "<p>WiFi Module: ISM43362</p>"                                             \
    "<p>Status: Connected and Running</p>"                                     \
    "</body></html>"

/* Private variables ---------------------------------------------------------*/
static uint8_t http_buffer[HTTP_BUFFER_SIZE];
static uint8_t ip_addr[4];

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void LED_Init(void);
static void WiFi_InitAndConnect(void);
static void HTTP_ServerProcess(void);

[[noreturn]]
int main(int argc, char *argv[]) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    Error_Handler_Init();

    trace_printf("\n\n");
    trace_printf("========================================\n");
    trace_printf("  STM32F413H WiFi HTTP Server\n");
    trace_printf("========================================\n");
    trace_printf("Initializing...\n\n");

    WiFi_InitAndConnect();

    trace_printf("\n========================================\n");
    trace_printf("  Server Ready!\n");
    trace_printf("========================================\n");
    trace_printf("Access the server at: http://%d.%d.%d.%d\n", ip_addr[0],
                 ip_addr[1], ip_addr[2], ip_addr[3]);
    trace_printf("Waiting for HTTP requests...\n\n");

    /* Blink green LED to indicate ready state */
    BSP_LED_On(LED_GREEN);

    while (1) {
        HTTP_ServerProcess();
        HAL_Delay(50);
    }
}

static void WiFi_InitAndConnect(void) {
    WIFI_Status_t status;

    trace_printf("Step 1: Initializing WiFi module...\n");
    status = WIFI_Init();
    if (status != WIFI_STATUS_OK) {
        trace_printf("ERROR: WiFi initialization failed! (Status: %d)\n",
                     status);
        Error_Handler("WiFi Init Failed");
    }
    trace_printf("  -> WiFi module initialized\n");

    trace_printf("\nStep 2: Connecting to network '%s'...\n", WIFI_SSID);
    status = WIFI_Connect(WIFI_SSID, WIFI_PASSWORD, WIFI_ECN_WPA2_PSK);
    if (status != WIFI_STATUS_OK) {
        trace_printf("ERROR: WiFi connection failed! (Status: %d)\n", status);
        trace_printf("  Check SSID and password in wifi_conf.hpp\n");
        Error_Handler("WiFi Connect Failed");
    }
    trace_printf("  -> Connected to WiFi network\n");

    trace_printf("\nStep 3: Getting IP address...\n");
    status = WIFI_GetIP_Address(ip_addr);
    if (status != WIFI_STATUS_OK) {
        trace_printf("ERROR: Failed to get IP address! (Status: %d)\n", status);
        Error_Handler("Get IP Failed");
    }
    trace_printf("  -> IP Address: %d.%d.%d.%d\n", ip_addr[0], ip_addr[1],
                 ip_addr[2], ip_addr[3]);

    trace_printf("\nStep 4: Starting HTTP server on port %d...\n",
                 HTTP_SERVER_PORT);
    status = WIFI_StartServer(0, WIFI_TCP_PROTOCOL, "HTTP", HTTP_SERVER_PORT);
    if (status != WIFI_STATUS_OK) {
        trace_printf("ERROR: Failed to start HTTP server! (Status: %d)\n",
                     status);
        Error_Handler("HTTP Server Start Failed");
    }
    trace_printf("  -> HTTP server started\n");
}

/**
 * @brief  Process HTTP requests
 * @param  None
 * @retval None
 */
static void HTTP_ServerProcess(void) {
    static uint32_t request_count = 0;
    static uint32_t restart_until = 0;
    uint16_t recv_len = 0;
    uint16_t sent_len = 0;
    WIFI_Status_t status;
    
    /* Skip receive if we're in restart period */
    if (HAL_GetTick() < restart_until) {
        return;  /* Still in restart period */
    }

    /* Try to receive data (non-blocking) */
    status = WIFI_ReceiveData(0, http_buffer, HTTP_BUFFER_SIZE - 1, &recv_len);

    /* Silently ignore socket errors - these are expected when no client is connected
     * or during socket state transitions after restart. Only process valid data. */
    if (status != WIFI_STATUS_OK || recv_len == 0) {
        return;  /* No data available or socket not ready */
    }

    /* We have valid data - process the request */
    if (recv_len > 0) {
        http_buffer[recv_len] = '\0';
        request_count++;

        char *request_line = strtok((char *)http_buffer, "\r\n");
        trace_printf("[%lu] %s\n", request_count, request_line ? request_line : "Invalid request");

        /* Toggle green LED on each request */
        BSP_LED_Toggle(LED_GREEN);

        /* Restore buffer for processing (strtok modified it) */
        http_buffer[recv_len] = '\0';

        /* Check if it's a GET request */
        if (strncmp((char *)http_buffer, "GET", 3) == 0) {
            char response[1024];
            snprintf(response, sizeof(response), "%s%s", HTTP_RESPONSE_HEADER,
                     HTML_PAGE);

            /* Send response */
            status = WIFI_SendData(0, (uint8_t *)response, strlen(response),
                                   &sent_len);

            if (status == WIFI_STATUS_OK) {
                trace_printf("[%lu] Sent response (%d bytes)\n", request_count, sent_len);
            } else {
                trace_printf("[%lu] ERROR: Failed to send response (Status: %d)\n",
                           request_count, status);
            }
        }

        /* Close the client connection properly */
        HAL_Delay(100);  /* Give time for client to receive response */
        WIFI_CloseClientConnection();
        
        restart_until = HAL_GetTick() + 300;
        
        HAL_Delay(100);
        WIFI_StopServer(0);
        HAL_Delay(100);
        status = WIFI_StartServer(0, WIFI_TCP_PROTOCOL, "HTTP", HTTP_SERVER_PORT);
        if (status != WIFI_STATUS_OK) {
            trace_printf("ERROR: Failed to restart server (Status: %d)\n", status);
        }
    }
}

static void LED_Init(void) {
    BSP_LED_Init(LED_GREEN);
    BSP_LED_Init(LED_RED);
    BSP_LED_Off(LED_GREEN);
    BSP_LED_Off(LED_RED);
}

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Use HSI directly (16 MHz) - simplest, most reliable configuration */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler("Clock Config Failed - HSI");
    }

    /* Configure CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // 16 MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;  // 16 MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  // 16 MHz

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler("Clock Config Failed - Buses");
    }

    /* CRITICAL: Configure SysTick to generate interrupts at 1kHz (1ms) */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);

    /* Set SysTick clock source */
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

#pragma GCC diagnostic pop
