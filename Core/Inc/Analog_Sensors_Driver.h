#ifndef ANALOG_SENSORS_DRIVER_H
#define ANALOG_SENSORS_DRIVER_H

#include "main.h"

// 模拟传感器数据结构
typedef struct {
    float water_level_percent; // 水位百分比 (0-100%)
    float gas_ppm;             // 气体浓度 (PPM)
} Analog_Data;

// 初始化ADC
void Analog_Sensors_Init(void);

// 读取模拟传感器数据
void Analog_Sensors_Read(Analog_Data *data);

#endif /* ANALOG_SENSORS_DRIVER_H */
