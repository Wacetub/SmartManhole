/* USER CODE BEGIN Header */ 
 /** 
   ****************************************************************************** 
   * @file           : main.c 
   * @brief          : Main program body 
   ****************************************************************************** 
   * @attention 
   * 
   * Copyright (c) 2026 STMicroelectronics. 
   * All rights reserved. 
   * 
   * This software is licensed under terms that can be found in the LICENSE file 
   * in the root directory of this software component. 
   * If no LICENSE file comes with this software, it is provided AS-IS. 
   * 
   ****************************************************************************** 
   */ 
 /* USER CODE END Header */ 
 /* Includes ------------------------------------------------------------------*/ 
 #include "main.h" 
 #include "adc.h" 
 #include "i2c.h" 
 #include "usart.h" 
 #include "gpio.h" 
 
 /* Private includes ----------------------------------------------------------*/ 
 /* USER CODE BEGIN Includes */ 
 #include <stdio.h>
 #include <string.h>
 #include "NB_IoT_Driver.h"
 #include "DHT11_Driver.h"
 #include "MPU6050_Driver.h"
 #include "Analog_Sensors_Driver.h"
 #include "GPS_Driver.h"
 /* USER CODE END Includes */ 
 
 /* Private typedef -----------------------------------------------------------*/ 
 /* USER CODE BEGIN PTD */ 
 // 模组传感器数据结构
 typedef struct {
     float tilt_angle;       // 倾斜角度
     float water_level;      // 水位
     float temperature;      // 温度
     float humidity;         // 湿度
     float gas_concentration;// 有害气体浓度
     double longitude;       // 经度
     double latitude;        // 纬度
     uint8_t status;         // 状态: 0-正常, 1-异常
 } ManholeStatus;
 /* USER CODE END PTD */ 
 
 /* Private define ------------------------------------------------------------*/ 
 /* USER CODE BEGIN PD */ 
 
 /* USER CODE END PD */ 
 
 /* Private macro -------------------------------------------------------------*/ 
 /* USER CODE BEGIN PM */ 
 
 /* USER CODE END PM */ 
 
 /* Private variables ---------------------------------------------------------*/ 
 
 /* USER CODE BEGIN PV */ 
 ManholeStatus current_status;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
 uint8_t gps_rx_byte;       // GPS接收缓冲字节
uint8_t gps_fix_valid = 0;
float g_alarm_tilt = 15.0f;
float g_alarm_water = 80.0f;
float g_alarm_gas = 500.0f;
 /* USER CODE END PV */ 
 
 /* Private function prototypes -----------------------------------------------*/ 
 void SystemClock_Config(void); 
 /* USER CODE BEGIN PFP */ 
 void Read_Sensors(void);
uint8_t NB_IoT_Send_Data(ManholeStatus *status);
 void Check_Alarm(ManholeStatus *status);
 extern void MX_USART2_UART_Init(void); // 在usart.c中定义
 extern void MX_USART3_UART_Init(void); // 在usart.c中定义
 /* USER CODE END PFP */ 
 
 /* Private user code ---------------------------------------------------------*/ 
 /* USER CODE BEGIN 0 */ 
int fputc(int ch, FILE *f) {
    if (ch == '\n') {
        uint8_t cr = '\r';
        HAL_UART_Transmit(&huart1, &cr, 1, 0xFFFF);
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
 
 void Read_Sensors(void) {
     DHT11_Data dht11_data;
     if (DHT11_Read_Data(&dht11_data) == 0) {
        current_status.temperature = dht11_data.temperature;
        current_status.humidity = dht11_data.humidity;
    } else {
        printf("DHT11 Read Error\n");
    }
 
     MPU6050_Data mpu_data;
     MPU6050_Read_Tilt(&mpu_data);
    current_status.tilt_angle = (mpu_data.Tilt_X > mpu_data.Tilt_Y) ? mpu_data.Tilt_X : mpu_data.Tilt_Y;
     
    Analog_Data analog_data;
    Analog_Sensors_Read(&analog_data);
    current_status.water_level = analog_data.water_level_percent;
    current_status.gas_concentration = analog_data.gas_ppm;

    printf("Sensors Read: Tilt=%.2f, Water=%.2f, Gas=%.2f\n", current_status.tilt_angle, current_status.water_level, current_status.gas_concentration);
}
 
uint8_t NB_IoT_Send_Data(ManholeStatus *status) {
     static char payload[512];
    if (gps_fix_valid) {
        snprintf(payload, sizeof(payload),
                 "{"
                 "\"temp\":%.1f,"
                 "\"humi\":%.1f,"
                 "\"tilt\":%.1f,"
                 "\"water\":%.1f,"
                 "\"gas\":%.1f,"
                 "\"lon\":%.6f,"
                 "\"lat\":%.6f"
                 "}",
                 status->temperature, status->humidity, status->tilt_angle, status->water_level, status->gas_concentration,
                 status->longitude, status->latitude);
    } else {
        snprintf(payload, sizeof(payload),
                 "{"
                 "\"temp\":%.1f,"
                 "\"humi\":%.1f,"
                 "\"tilt\":%.1f,"
                 "\"water\":%.1f,"
                 "\"gas\":%.1f"
                 "}",
                 status->temperature, status->humidity, status->tilt_angle, status->water_level, status->gas_concentration);
    }
     
    if (!NB_IoT_Publish_Data(payload)) {
        printf("[NB-IoT] Publish failed.\n");
        return 0;
    }
    
    printf("Sending Data to OneNET (MQTT): %s\n", payload);
   return 1;
}
 
 void Check_Alarm(ManholeStatus *status) {
    static uint32_t last_blink_tick = 0;
    static GPIO_PinState blink_state = GPIO_PIN_RESET;
    uint8_t alarm_triggered = 0;
    uint32_t now = HAL_GetTick();

    // 统一 1s 亮 / 1s 灭节拍
    if (now - last_blink_tick >= 1000U) {
        last_blink_tick = now;
        blink_state = (blink_state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }

    if (status->tilt_angle > g_alarm_tilt) {
        alarm_triggered = 1;
        HAL_GPIO_WritePin(LED_TILT_GPIO_Port, LED_TILT_Pin, blink_state);
    } else {
        HAL_GPIO_WritePin(LED_TILT_GPIO_Port, LED_TILT_Pin, GPIO_PIN_RESET);
    }

    if (status->water_level > g_alarm_water) {
        alarm_triggered = 1;
        HAL_GPIO_WritePin(LED_WATER_GPIO_Port, LED_WATER_Pin, blink_state);
    } else {
        HAL_GPIO_WritePin(LED_WATER_GPIO_Port, LED_WATER_Pin, GPIO_PIN_RESET);
    }

    if (status->gas_concentration >= g_alarm_gas) {
        alarm_triggered = 1;
        HAL_GPIO_WritePin(LED_GAS_GPIO_Port, LED_GAS_Pin, blink_state);
    } else {
        HAL_GPIO_WritePin(LED_GAS_GPIO_Port, LED_GAS_Pin, GPIO_PIN_RESET);
    }

    // 串口输出实时判定细节，便于确认阈值是否已下发到设备端
    printf("Alarm Check: Gas=%.1f, Threshold_Gas=%.1f, GasAlarm=%d\n",
           status->gas_concentration, g_alarm_gas, (status->gas_concentration >= g_alarm_gas) ? 1 : 0);

    status->status = alarm_triggered ? 1 : 0;
 }
 /* USER CODE END 0 */ 
 
 /** 
   * @brief  The application entry point. 
   * @retval int 
   */ 
 int main(void) 
 { 
 
   /* USER CODE BEGIN 1 */ 
 
   /* USER CODE END 1 */ 
 
   /* MCU Configuration--------------------------------------------------------*/ 
 
   /* Reset of all peripherals, Initializes the Flash interface and the Systick. */ 
   HAL_Init(); 
 
   /* USER CODE BEGIN Init */ 
 
   /* USER CODE END Init */ 
 
   /* Configure the system clock */ 
   SystemClock_Config(); 
 
   /* USER CODE BEGIN SysInit */ 
 
   /* USER CODE END SysInit */ 
 
   /* Initialize all configured peripherals */ 
   MX_GPIO_Init(); 
   MX_ADC1_Init(); 
   MX_I2C1_Init(); 
   MX_USART1_UART_Init(); 
   MX_USART2_UART_Init();
   MX_USART3_UART_Init();
   
   /* USER CODE BEGIN 2 */ 
   printf("\r\n--- Smart Manhole System Starting ---\r\n");
   
   HAL_UART_Receive_IT(&huart3, &gps_rx_byte, 1);
   
   printf("Waiting for NB-IoT Module to boot up...\r\n");
   HAL_Delay(3000);
   NB_IoT_Init();
  NB_IoT_Subscribe_Downlink();
     
   GPS_Init();
 
   DHT11_Init();
   MPU6050_Init();
   Analog_Sensors_Init();
   
   /* USER CODE END 2 */ 
 
   /* Infinite loop */ 
   /* USER CODE BEGIN WHILE */ 
  uint32_t last_ping_time = HAL_GetTick();
  uint32_t last_sample_time = 0;
  uint32_t last_threshold_poll_time = 0;
  uint8_t nb_send_fail_count = 0;
   
   while (1) 
   { 
     /* USER CODE END WHILE */ 
 
     /* USER CODE BEGIN 3 */ 
      // LED 报警节拍独立运行：1s 亮 / 1s 灭
      Check_Alarm(&current_status);

      // 采集与上报改回 2s 周期，避免 NB-IoT 串口阻塞导致链路不稳定
      if (HAL_GetTick() - last_sample_time >= 2000U) {
          last_sample_time = HAL_GetTick();

          Read_Sensors();

          GPS_Data gps_data;
          GPS_Get_Location(&gps_data);
          if (gps_data.is_valid) {
              current_status.longitude = gps_data.longitude;
              current_status.latitude = gps_data.latitude;
              gps_fix_valid = 1;
          }

          if (NB_IoT_Send_Data(&current_status)) {
              nb_send_fail_count = 0;
          } else {
              nb_send_fail_count++;
              if (nb_send_fail_count >= 3) {
                  printf("[NB-IoT] Consecutive publish failures, reconnecting MQTT...\n");
                  NB_IoT_Init();
                  NB_IoT_Subscribe_Downlink();
                  nb_send_fail_count = 0;
              }
          }

      }

      // 下行阈值轮询降频到 5s，避免每次上报后立即 AT+NSORF 造成串口拥塞
      if (HAL_GetTick() - last_threshold_poll_time >= 5000U) {
          last_threshold_poll_time = HAL_GetTick();
          if (NB_IoT_Poll_Threshold_Command(&g_alarm_tilt, &g_alarm_water, &g_alarm_gas)) {
              printf("Alarm threshold changed: tilt=%.1f, water=%.1f, gas=%.1f\n",
                     g_alarm_tilt, g_alarm_water, g_alarm_gas);
          }
      }

      if (HAL_GetTick() - last_ping_time > 60000U) {
          NB_IoT_Send_Ping();
          last_ping_time = HAL_GetTick();
      }

      HAL_Delay(50);
   } 
   /* USER CODE END 3 */ 
 } 
 
 /** 
   * @brief System Clock Configuration 
   * @retval None 
   */ 
 void SystemClock_Config(void) 
 { 
   RCC_OscInitTypeDef RCC_OscInitStruct = {0}; 
   RCC_ClkInitTypeDef RCC_ClkInitStruct = {0}; 
   RCC_PeriphCLKInitTypeDef PeriphClkInit = {0}; 
 
   /** Initializes the RCC Oscillators according to the specified parameters 
   * in the RCC_OscInitTypeDef structure. 
   */ 
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE; 
   RCC_OscInitStruct.HSEState = RCC_HSE_ON; 
   RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1; 
   RCC_OscInitStruct.HSIState = RCC_HSI_ON; 
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON; 
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE; 
   RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9; 
   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) 
   { 
     Error_Handler(); 
   } 
 
   /** Initializes the CPU, AHB and APB buses clocks 
   */ 
   RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK 
                               |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2; 
   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; 
   RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; 
   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; 
   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; 
 
   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) 
   { 
     Error_Handler(); 
   } 
   PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC; 
   PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6; 
   if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) 
   { 
     Error_Handler(); 
   } 
 } 
 
 /* USER CODE BEGIN 4 */ 
 
 void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
     if (huart->Instance == USART3) {
         GPS_Process_Data(gps_rx_byte);
         HAL_UART_Receive_IT(&huart3, &gps_rx_byte, 1);
     }
 }

 /** 
   * @brief  This function is executed in case of error occurrence. 
   * @retval None 
   */ 
 void Error_Handler(void) 
 { 
   /* USER CODE BEGIN Error_Handler_Debug */ 
   /* User can add his own implementation to report the HAL error return state */ 
   __disable_irq(); 
   while (1) 
   { 
   } 
   /* USER CODE END Error_Handler_Debug */ 
 } 
 
 #ifdef  USE_FULL_ASSERT 
 /** 
   * @brief  Reports the name of the source file and the source line number 
   *         where the assert_param error has occurred. 
   * @param  file: pointer to the source file name 
   * @param  line: assert_param error line source number 
   * @retval None 
   */ 
 void assert_failed(uint8_t *file, uint32_t line) 
 { 
   /* USER CODE BEGIN 6 */ 
   /* User can add his own implementation to report the file name and line number, 
      ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */ 
   /* USER CODE END 6 */ 
 } 
 #endif /* USE_FULL_ASSERT */ 
