#include "MPU6050_Driver.h"
#include <math.h>
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

// I2C 封装函数
void MPU6050_Write(uint8_t reg_addr, uint8_t data) {
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, reg_addr, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

void MPU6050_Read(uint8_t reg_addr, uint8_t *pData, uint16_t size) {
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, reg_addr, I2C_MEMADD_SIZE_8BIT, pData, size, 100);
}

void MPU6050_Init(void) {
    uint8_t check;
    uint8_t data;

    // 1. 检查设备ID (WHO_AM_I 寄存器 0x75)
    // 如果返回 0x68 或者 0x70，都认为是正常的 MPU6050
    MPU6050_Read(0x75, &check, 1);

    if (check == 0x68 || check == 0x70) {
        // 2. 唤醒传感器 (PWR_MGMT_1 寄存器写 0)
        data = 0;
        MPU6050_Write(MPU6050_REG_PWR_MGMT_1, data);
        printf("MPU6050 Initialized Successfully (ID: 0x%02X)\n", check);
    } else {
        printf("MPU6050 Init Failed (ID: 0x%02X)\n", check);
    }
}

void MPU6050_Read_Tilt(MPU6050_Data *data) {
    uint8_t Rec_Data[6];
    int16_t Accel_X_RAW, Accel_Y_RAW, Accel_Z_RAW;

    // 1. 读取加速度计原始数据 (6字节: X_H, X_L, Y_H, Y_L, Z_H, Z_L)
    MPU6050_Read(MPU6050_REG_ACCEL_XOUT_H, Rec_Data, 6);
    
    // 合成数据
    Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    // 2. 转换为重力加速度单位 (g)
    data->Accel_X = Accel_X_RAW / 16384.0;
    data->Accel_Y = Accel_Y_RAW / 16384.0;
    data->Accel_Z = Accel_Z_RAW / 16384.0;

    // 3. 计算倾角 (使用 atan2 函数)
    // 限制范围在 -180 到 180 度
    // 57.29578 是 180 / PI
    data->Tilt_X = atan2(data->Accel_Y, sqrt(pow(data->Accel_X, 2) + pow(data->Accel_Z, 2))) * 57.29578;
    data->Tilt_Y = atan2(-data->Accel_X, sqrt(pow(data->Accel_Y, 2) + pow(data->Accel_Z, 2))) * 57.29578;

    // printf("MPU6050 Read: Tilt X=%.2f, Y=%.2f\n", data->Tilt_X, data->Tilt_Y);
}
