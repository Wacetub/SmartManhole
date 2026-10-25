#include "DHT11_Driver.h"
#include "gpio.h"
#include <stdio.h>

// Use unified pin definitions from main.h via DHT11_Driver.h
#define DHT11_PORT DHT11_GPIO_Port
#define DHT11_PIN  DHT11_Pin

// 微秒延时函数 (使用简单的循环，假设主频 72MHz)
// 也可以使用 DWT 或 定时器 实现更精确的延时
void delay_us(uint32_t us) {
    uint32_t i;
    // 粗略估算: 72MHz下，循环大约需要 72个时钟周期
    // 这里用经验值，可能需要根据编译器优化级别调整
    while(us--) {
        for(i=0; i<10; i++) {
            __NOP();
        }
    }
}

// 设置GPIO模式: 输出
void DHT11_Set_Output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// 设置GPIO模式: 输入
void DHT11_Set_Input(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

void DHT11_Init(void) {
    DHT11_Set_Output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    HAL_Delay(1000); // 上电稳定等待
    printf("DHT11 Initialized\n");
}

uint8_t DHT11_Read_Data(DHT11_Data *data) {
    uint8_t i, j;
    uint8_t buffer[5] = {0};
    
    // 1. 主机发送开始信号
    DHT11_Set_Output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20); // 拉低至少18ms
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    delay_us(30);  // 拉高20-40us

    // 2. 主机设为输入模式，等待DHT11响应
    DHT11_Set_Input();
    
    // 检查DHT11是否拉低响应 (等待80us低电平)
    if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET) {
        
        // 等待DHT11响应的低电平结束
        while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET);
        
        // 等待DHT11响应的高电平结束
        while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET);

        // 3. 读取40位数据
        for (i = 0; i < 5; i++) {
            for (j = 0; j < 8; j++) {
                // 等待数据位开始 (50us低电平)
                while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET);
                
                // 延时40us判断高低电平 (0: 26-28us高, 1: 70us高)
                delay_us(40);
                
                if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET) {
                    buffer[i] |= (1 << (7 - j));
                    // 等待数据位结束
                    while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET);
                }
            }
        }
        
        // 4. 校验
        if ((buffer[0] + buffer[1] + buffer[2] + buffer[3]) == buffer[4]) {
            data->humidity = (float)buffer[0] + (float)buffer[1]/10.0;
            data->temperature = (float)buffer[2] + (float)buffer[3]/10.0;
            // printf("DHT11 Read Success: Temp=%.1f, Humi=%.1f\n", data->temperature, data->humidity);
            return 0; // 成功
        }
    }

    // 读取失败
    return 1;
}
