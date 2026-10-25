#ifndef __GPS_DRIVER_H
#define __GPS_DRIVER_H

#include "stm32f1xx_hal.h"

// GPS数据结构体
typedef struct {
    double longitude;   // 经度
    double latitude;    // 纬度
    uint8_t is_valid;   // 定位是否有效 (0:无效, 1:有效)
} GPS_Data;

// 函数原型
void GPS_Init(void);
void GPS_Process_Data(uint8_t byte);
void GPS_Get_Location(GPS_Data *data);

#endif
