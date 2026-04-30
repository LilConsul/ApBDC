#include "diag/trace.h"
#include "stm32f413h_discovery.h"
#include "stm32f4xx_hal.h"
#include "wifi.h"
#include "es_wifi_io.h"
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

/* Server state machine states */
typedef enum {
    SERVER_STATE_IDLE = 0,
    SERVER_STATE_PROCESSING,
    SERVER_STATE_SENT_RESPONSE,
    SERVER_STATE_CLOSING,
    SERVER_STATE_RESTARTING
} ServerState_t;

/* Private variables ---------------------------------------------------------*/
static uint8_t http_buffer[HTTP_BUFFER_SIZE];
static uint8_t ip_addr[4];
static ServerState_t server_state = SERVER_STATE_IDLE;
static uint32_t request_count = 0;
static uint8_t restart_step = 0;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void LED_Init(void);
static void WiFi_InitAndConnect(void);
static void HTTP_ServerProcess(void);
static void HTTP_ServerMaintenance(void);

[[noreturn]]
int main(int argc, char *argv[]) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    Error_Handler_Init();

    trace_printf("\n\n");
    trace_printf("========================================\n");
    trace_printf("  STM32F413H WiFi HTTP Server\n");
    trace_printf("  (Interrupt-Driven Architecture)\n");
    trace_printf("========================================\n");
    trace_printf("Initializing...\n\n");

    WiFi_InitAndConnect();

    trace_printf("\n========================================\n");
    trace_printf("  Server Ready!\n");
    trace_printf("========================================\n");
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
        
        if (server_maintenance_flag) {
            server_maintenance_flag = 0;
            HTTP_ServerMaintenance();
        }
        
        __WFI();  /* Wait For Interrupt */
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
 * @brief  Process HTTP requests (interrupt-driven, non-blocking)
 * @param  None
 * @retval None
 */
static void HTTP_ServerProcess(void) {
    uint16_t recv_len = 0;
    uint16_t sent_len = 0;
    WIFI_Status_t status;
    
    /* Only process if in idle state */
    if (server_state != SERVER_STATE_IDLE) {
        return;  /* Server busy with maintenance */
    }
    
    /* Try to receive data (non-blocking) */
    status = WIFI_ReceiveData(0, http_buffer, HTTP_BUFFER_SIZE - 1, &recv_len);

    /* Silently ignore socket errors */
    if (status != WIFI_STATUS_OK || recv_len == 0) {
        return;
    }

    /* We have valid data - check what it is */
    if (recv_len > 0) {
        http_buffer[recv_len] = '\0';
        
        /* Filter out WiFi module status messages */
        if (strstr((char *)http_buffer, "[SOMA]") != NULL ||
            strstr((char *)http_buffer, "[EOMA]") != NULL ||
            strstr((char *)http_buffer, "Unhandled Socket") != NULL ||
            strstr((char *)http_buffer, "TCP SVR") != NULL ||
            strcmp((char *)http_buffer, "-1") == 0 ||
            (recv_len <= 3 && http_buffer[0] == '-')) {
            return;  /* Silently ignore status messages */
        }
        
        /* This appears to be an actual HTTP request */
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
            
            /* Move to sent response state and set timer for connection close */
            server_state = SERVER_STATE_SENT_RESPONSE;
            maintenance_timer = 100;  /* 100ms delay to allow data transmission */
        }
    }
}

/**
 * @brief  Perform server maintenance tasks (timer-driven)
 * @param  None
 * @retval None
 */
static void HTTP_ServerMaintenance(void) {
    WIFI_Status_t status;
    
    /* Handle state transitions based on current state */
    switch (server_state) {
        case SERVER_STATE_IDLE:
            /* Nothing to do in idle state */
            break;

        case SERVER_STATE_SENT_RESPONSE:
            /* Close client connection after response sent */
            WIFI_CloseClientConnection();
            trace_printf("  -> Connection closed\n");
            
            /* Move to closing state and set timer */
            server_state = SERVER_STATE_CLOSING;
            maintenance_timer = 100;  /* 100ms delay before stopping server */
            break;

        case SERVER_STATE_CLOSING:
            /* Stop server */
            WIFI_StopServer(0);
            trace_printf("  -> Server stopped\n");
            
            /* Move to restarting state and set timer */
            server_state = SERVER_STATE_RESTARTING;
            maintenance_timer = 100;  /* 100ms delay before restarting */
            break;

        case SERVER_STATE_RESTARTING:
            /* Restart server */
            status = WIFI_StartServer(0, WIFI_TCP_PROTOCOL, "HTTP", HTTP_SERVER_PORT);
            if (status != WIFI_STATUS_OK) {
                trace_printf("ERROR: Failed to restart server (Status: %d)\n", status);
            } else {
                trace_printf("  -> Server restarted\n");
            }
            
            /* Return to idle state */
            server_state = SERVER_STATE_IDLE;
            break;

        default:
            server_state = SERVER_STATE_IDLE;
            break;
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
