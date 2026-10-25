#ifndef DHT11_DRIVER_H
#define DHT11_DRIVER_H

#include "main.h"

// DHT11 数据结构
typedef struct {
    float temperature;
    float humidity;
} DHT11_Data;

// 初始化函数
void DHT11_Init(void);

// 读取温湿度函数
// 返回值: 0-成功, 1-失败
uint8_t DHT11_Read_Data(DHT11_Data *data);

#endif /* DHT11_DRIVER_H */
