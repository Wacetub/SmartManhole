#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#include "main.h"

// MPU6050 I2C地址 (根据AD0引脚电平可能为0xD0或0xD2)
#define MPU6050_ADDR 0xD0

// MPU6050 寄存器地址
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

// 倾角数据结构
typedef struct {
    float Accel_X;
    float Accel_Y;
    float Accel_Z;
    float Tilt_X; // X轴倾角 (度)
    float Tilt_Y; // Y轴倾角 (度)
} MPU6050_Data;

// 初始化MPU6050
void MPU6050_Init(void);

// 读取加速度并计算倾角
void MPU6050_Read_Tilt(MPU6050_Data *data);

#endif /* MPU6050_DRIVER_H */
