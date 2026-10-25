#include "Analog_Sensors_Driver.h"
#include <stdio.h>

extern ADC_HandleTypeDef hadc1;

void Analog_Sensors_Init(void) {
    // 开启ADC校准
    HAL_ADCEx_Calibration_Start(&hadc1);
    printf("Analog Sensors (ADC) Initialized\n");
}

uint32_t Get_ADC_Value(uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5; // 增加采样时间提高稳定性
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }

    uint32_t total_adc = 0;
    const int sample_count = 10; // 采集10次求平均，软件滤波抗干扰

    for (int i = 0; i < sample_count; i++) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            total_adc += HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);
    }
    
    return total_adc / sample_count;
}

void Analog_Sensors_Read(Analog_Data *data) {
    // 1. 读取水位传感器 (连接在 PA4 -> ADC1_IN4)
    uint32_t water_adc = Get_ADC_Value(ADC_CHANNEL_4);
    
    // XU5-1 水位传感器多点分段线性校准逻辑 (解决非线性跳变问题)
    // 请通过串口查看实际水深对应的 water_adc 值，并修改下表的 adc_val
    typedef struct {
        uint32_t adc_val;
        float percent;
    } WaterCalPoint;

    // 校准表（必须保证 adc_val 从小到大排列）
    const WaterCalPoint cal_table[] = {
        {17,     0.0f},   // 完全没水 (死区)
        {1889,  20.6f},   // 实测点1
        {2121,  32.6f},   // 实测点2
        {2815,  75.9f},   // 实测点3
        {3223, 100.0f}    // 完全淹没到最高线
    };
    const int num_points = sizeof(cal_table) / sizeof(cal_table[0]);

    if (water_adc <= cal_table[0].adc_val) {
        data->water_level_percent = cal_table[0].percent;
    } else if (water_adc >= cal_table[num_points - 1].adc_val) {
        data->water_level_percent = cal_table[num_points - 1].percent;
    } else {
        // 在两点之间进行线性插值计算
        for (int i = 0; i < num_points - 1; i++) {
            if (water_adc >= cal_table[i].adc_val && water_adc <= cal_table[i+1].adc_val) {
                float adc_diff = cal_table[i+1].adc_val - cal_table[i].adc_val;
                float pct_diff = cal_table[i+1].percent - cal_table[i].percent;
                float proportion = (water_adc - cal_table[i].adc_val) / adc_diff;
                data->water_level_percent = cal_table[i].percent + (proportion * pct_diff);
                break;
            }
        }
    }

    // 单点比例修正: 现场观测 80% 实际水位约对应 5.7% 读数
    // scale = 80 / 5.7 ≈ 14.035，先用快速修正，后续建议重做多点标定
    {
        const float water_scale = 14.0351f;
        data->water_level_percent *= water_scale;
        if (data->water_level_percent > 100.0f) data->water_level_percent = 100.0f;
        if (data->water_level_percent < 0.0f) data->water_level_percent = 0.0f;
    }

    // 2. 读取气体传感器 (连接在 PA5 -> ADC1_IN5)
    // 推荐型号: MQ-4 (甲烷) 或 MQ-135 (空气质量)
    uint32_t gas_adc = Get_ADC_Value(ADC_CHANNEL_5);
    
    // 简单转换: 假设传感器输出电压与PPM成正比 (实际需参考传感器曲线)
    data->gas_ppm = (gas_adc / 4095.0f) * 1000.0f; // 假设满量程1000PPM

    printf("Analog Sensors Read: Water_ADC=%lu -> %.1f%%, Gas=%.1f PPM\n", water_adc, data->water_level_percent, data->gas_ppm);
}
