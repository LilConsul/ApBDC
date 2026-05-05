#include "diag/trace.h"
#include "es_wifi_io.h"
#include "http_client.h"
#include "sensor/magnetometer_stream.h"
#include "stm32f4xx_hal.h"
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

/* Private variables ---------------------------------------------------------*/
static uint8_t http_buffer[HTTP_BUFFER_SIZE];
static uint8_t server_ip[4] = SERVER_IP_BYTES;

/* Flag set by TIM3 ISR to trigger an HTTP POST */
volatile uint8_t http_send_flag = 0;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void LED_Init(void);
static void TIM3_Init(void);
static void WiFi_InitAndConnect(void);
static void HTTP_SendHeading(void);
static float simple_atan2(float y, float x);

[[noreturn]]
int main(int argc, char *argv[]) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    Error_Handler_Init();

    trace_printf("\n\n");
    trace_printf("================================\n");
    trace_printf("  STM32F413H WiFi HTTP Client\n");
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

    trace_printf("\nStep 4: Starting magnetometer sampling...\n");
    Magnetometer_StartSampling();
    trace_printf("  -> Magnetometer sampling started at 20Hz\n");

    trace_printf("\nStep 5: Starting HTTP send timer (every %d ms)...\n",
                 HTTP_SEND_INTERVAL_MS);
    TIM3_Init();
    trace_printf("  -> TIM3 configured for %d ms periodic HTTP POST\n",
                 HTTP_SEND_INTERVAL_MS);

    trace_printf("\n======================\n");
    trace_printf("  Client Ready!\n");
    trace_printf("======================\n");
    trace_printf("Sending heading to http://%d.%d.%d.%d:%d%s\n",
                 server_ip[0], server_ip[1], server_ip[2], server_ip[3],
                 SERVER_PORT, SERVER_ENDPOINT);
    trace_printf("CPU will enter low-power mode (WFI) between sends\n\n");

    BSP_LED_On(LED_GREEN);

    while (1) {
        if (http_send_flag) {
            http_send_flag = 0;
            HTTP_SendHeading();
        }

        __WFI();
    }
}

/**
 * @brief  Connect to WiFi network (no server startup)
 */
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
}

/**
 * @brief  Build HTTP POST body and send heading to the Python server
 */
static void HTTP_SendHeading(void) {
    MagSample_t sample;
    char degree_str[16] = "0.0";
    char body[32];
    int body_len;
    int request_len;
    uint16_t sent_len = 0;
    WIFI_Status_t status;

    BSP_LED_Off(LED_GREEN);

    /* Read the latest magnetometer sample */
    if (Magnetometer_GetAvailableSamples() > 0 &&
        Magnetometer_ReadSample(&sample)) {
        float heading =
            simple_atan2(sample.y, sample.x) * 180.0f / 3.14159265f;
        if (heading < 0) {
            heading += 360.0f;
        }
        snprintf(degree_str, sizeof(degree_str), "%.1f", heading);
    }

    /* Build POST body: "degree=<value>" */
    body_len = snprintf(body, sizeof(body), "degree=%s", degree_str);

    /* Build server IP string for the Host header */
    char host_str[24];
    snprintf(host_str, sizeof(host_str), "%d.%d.%d.%d",
             server_ip[0], server_ip[1], server_ip[2], server_ip[3]);

    /* Build full HTTP POST request */
    request_len = snprintf((char *)http_buffer, HTTP_BUFFER_SIZE,
                           HTTP_POST_REQUEST_FMT,
                           host_str,
                           body_len,
                           degree_str);

    trace_printf("[SEND] Heading: %s deg -> %s:%d\n", degree_str, host_str,
                 SERVER_PORT);

    /* Open TCP connection to server */
    status = WIFI_OpenClientConnection(0, WIFI_TCP_PROTOCOL, "HTTP",
                                       (char *)server_ip, SERVER_PORT, 0);
    if (status != WIFI_STATUS_OK) {
        trace_printf("[SEND] ERROR: Failed to open connection (Status: %d)\n",
                     status);
        BSP_LED_On(LED_GREEN);
        return;
    }

    /* Send HTTP POST request */
    status = WIFI_SendData(0, http_buffer, (uint16_t)request_len, &sent_len);
    if (status == WIFI_STATUS_OK) {
        trace_printf("[SEND] OK (%d bytes)\n", sent_len);
    } else {
        trace_printf("[SEND] ERROR: Send failed (Status: %d)\n", status);
    }

    /* Close TCP connection */
    WIFI_CloseClientConnection();

    BSP_LED_On(LED_GREEN);
}

/**
 * @brief  Initialize TIM3 to fire every HTTP_SEND_INTERVAL_MS milliseconds
 */
static void TIM3_Init(void) {
    static TIM_HandleTypeDef htim3; /* static: handle must outlive this function */
    memset(&htim3, 0, sizeof(htim3));

    /* TIM3 clock: 16 MHz, prescaler 16000-1 → 1 kHz tick */
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = (16000 - 1);
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = (HTTP_SEND_INTERVAL_MS - 1);
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
        Error_Handler("TIM3 Init Failed");
    }

    HAL_NVIC_SetPriority(TIM3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    HAL_TIM_Base_Start_IT(&htim3);
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
    float abs_y = (y < 0) ? -y : y;
    float angle;

    if (x >= 0) {
        float r = (x - abs_y) / (x + abs_y);
        angle = 0.785398163f - 0.785398163f * r;
    } else {
        float r = (x + abs_y) / (abs_y - x);
        angle = 2.356194490f - 0.785398163f * r;
    }

    return (y < 0) ? -angle : angle;
}

#pragma GCC diagnostic pop
