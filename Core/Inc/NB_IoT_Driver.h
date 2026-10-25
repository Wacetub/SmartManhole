#ifndef NB_IOT_DRIVER_H
#define NB_IOT_DRIVER_H

#include <stdint.h> 

// --- 旧版 OneNET MQTT 接入参数 ---
#define ONENET_MQTT_SERVER "183.230.40.39"
#define ONENET_MQTT_PORT   6002

// 请替换为你自己的 OneNET 参数（勿提交真实值）
#define ONENET_PROJ_ID    "YOUR_PRODUCT_ID"        // 产品ID
#define ONENET_DEV_ID     "YOUR_DEVICE_ID"    // 设备ID
#define ONENET_AUTH_INFO  "YOUR_AUTH_INFO"      // 鉴权信息

// --- 函数声明 ---
void NB_IoT_Init(void);
void NB_IoT_Connect_OneNET_MQTT(void);
void NB_IoT_Send_AT_Command(const char* cmd);
uint8_t NB_IoT_Send_TCP_Data(uint8_t *data, uint16_t len);
uint8_t NB_IoT_Publish_Data(const char* payload);
void NB_IoT_Send_Ping(void);
void NB_IoT_Subscribe_Downlink(void);
uint8_t NB_IoT_Poll_Threshold_Command(float *tilt, float *water, float *gas);

#endif
