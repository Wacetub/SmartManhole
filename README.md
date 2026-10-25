# SmartManhole —— 智能井盖状态监测系统

基于 STM32F103C8T6 + NB-IoT 的井盖状态监测终端。采集井盖倾角、井下积水深度、有害气体浓度、温湿度与北斗/GPS 定位，通过 MN316 NB-IoT 模组以 **MQTT** 协议上报 OneNET 云平台；支持云端下发阈值、设备端即时生效，并配套微信小程序实现三页监控与自研三层加权洪涝风险评估。

> 2026 届本科毕业设计，获评**校级优秀毕业设计**（榆林大学 物联网工程）。
> 作者：吴佳君　指导教师：高燕　答辩：2026-05-16

---

## 1. 功能概览

| 能力 | 实现方式 |
| --- | --- |
| 井盖倾角监测 | MPU6050（I²C1，PB6/PB7），三轴加速度 `atan2` 解算倾角，取 X/Y 较大值 |
| 井下积水监测 | YW01 电阻式液位（ADC1_IN4 / PA4），5 点分段线性标定 + 10 次均值滤波 |
| 有害气体监测 | MQ-135（ADC1_IN5 / PA5），按满量程线性换算 ppm |
| 温湿度 | DHT11（PA1，单总线时序，40 bit + 校验） |
| 定位 | ATGM336H 北斗/GPS 双模（USART3，PB10/PB11），解析 GNRMC/GPRMC，仅 `A` 有效时更新 |
| 无线上云 | MN316 NB-IoT（USART2，PA2/PA3），AT 指令建 TCP 连接，**手工组包 MQTT** CONNECT/PUBLISH/SUBSCRIBE/PINGREQ |
| 本地告警 | 三路 LED（PB13 倾斜 / PB14 水位 / PB15 气体），1 s 闪烁节拍 |
| 阈值下发 | 设备订阅下行 Topic，`AT+NSORF` 读取载荷并解析 JSON，就地更新阈值 |
| 供电 | 太阳能板 + 18650 锂电池 + 5 V/2 A 充放电管理模块 |

## 2. 目录结构

```
SmartManhole/
├── Core/
│   ├── Inc/            # 外设与应用头文件（含 main.h 引脚宏）
│   └── Src/            # main.c、外设初始化、各传感器与 NB-IoT 驱动
├── Drivers/            # ST HAL 库（按需精简）+ CMSIS Core / Device 头文件
├── MDK-ARM/            # Keil MDK 工程（.uvprojx / .uvoptx / startup）
├── SmartManhole.ioc    # STM32CubeMX 工程
├── docs/               # 接线表 / 引脚表、毕业设计精简版
├── README.md
└── .gitignore
```

## 3. 硬件与接线

主控 **STM32F103C8T6**（Cortex-M3，72 MHz，64 KB Flash / 20 KB SRAM）。

| 模块 | 型号 | 接口 | STM32 引脚 |
| --- | --- | --- | --- |
| 温湿度 | DHT11 | 单总线 | PA1 |
| NB-IoT | MN316 | USART2 9600 | PA2 (TX) / PA3 (RX) |
| 水位 | YW01 | ADC1_IN4 | PA4 |
| 气体 | MQ-135 | ADC1_IN5 | PA5 |
| 倾角 | MPU6050 | I²C1 100 kHz | PB6 (SCL) / PB7 (SDA) |
| 定位 | ATGM336H | USART3 9600 | PB10 (TX) / PB11 (RX) |
| 调试 | USB-TTL | USART1 115200 | PA9 (TX) / PA10 (RX) |
| 报警 | LED × 3 | GPIO 推挽 | PB13 / PB14 / PB15 |

完整接线表（含电源、注意事项、阈值默认值）见 `docs/智能井盖-接线表与引脚表.pdf`。

## 4. 编译与烧写

1. 安装 **Keil MDK 5** + STM32F1 器件支持包；
2. 用 **STM32CubeMX** 打开 `SmartManhole.ioc`，可重新生成 `Drivers/` 与 `MDK-ARM` 工程（Toolchain 选 MDK-ARM V5）；
3. 打开 `MDK-ARM/SmartManhole.uvprojx`，编译（`F7`）；
4. 用 ST-Link（SWD）或串口 ISP 烧写；复位后 USART1 以 115200 输出启动日志。

## 5. OneNET MQTT 参数配置

连接参数集中在 `Core/Inc/NB_IoT_Driver.h`，仓库中为**占位值**，请替换为自己的产品/设备信息：

```c
#define ONENET_MQTT_SERVER "183.230.40.39"   // OneNET 旧版 MQTT 接入地址
#define ONENET_MQTT_PORT   6002
#define ONENET_PROJ_ID    "YOUR_PRODUCT_ID"
#define ONENET_DEV_ID     "YOUR_DEVICE_ID"
#define ONENET_AUTH_INFO  "YOUR_AUTH_INFO"
```

> ⚠️ 请勿把真实的设备 ID / 鉴权信息提交到公开仓库。推荐新建 `Core/Inc/nb_iot_config.h`（已被 `.gitignore` 忽略）存放真实值，并从 `NB_IoT_Driver.h` 中 `#include` 它。

## 6. 上报协议

设备每 **2 s** 采集并上报一帧 JSON（MQTT PUBLISH，QoS 1）：

```json
{"temp":26.3,"humi":58.0,"tilt":3.2,"water":12.5,"gas":180.0,"lon":109.72,"lat":38.28}
```

- GPS 未定位（NMEA 状态位 ≠ `A`）时**不携带** `lon`/`lat` 字段，避免云端存入 `0,0` 无效坐标；
- 心跳 `PINGREQ` 每 **60 s** 一次；
- 下行阈值轮询每 **5 s** 一次，载荷形如 `{"threshold_tilt":15,"threshold_water":80,"threshold_gas":500}`；
- 连续 3 次发布失败自动重连（重新建 TCP + MQTT CONNECT + SUBSCRIBE）。

默认报警阈值：倾角 **15°**、水位 **80 %**、气体 **500 ppm**（可在小程序端远程修改）。

## 7. 已实现 / 待改进

**已实现**：多传感器周期采集与滤波、NMEA 解析与有效性判定、JSON 封装与 MQTT 上下行、断线重连、三路本地告警、阈值远程生效。

**待改进**：
- 低功耗：当前为周期全量上报，未做 STOP/待机模式与“变化才上报”策略；
- 数据补传：断网期间无本地缓存，恢复后不补传；
- 倾角解算：目前用加速度静态解算，未融合陀螺仪与卡尔曼滤波；
- 气体标定：为满量程线性映射，未按传感器曲线做多点标定与温湿度补偿；
- 结构：原型为面包板 + 杜邦线，量产需做防水防压壳体与 PCB。

## 8. 相关文档

- 毕业设计精简版（作品集用）：`docs/毕业设计精简版.pdf`
- 接线表 / 引脚表：`docs/智能井盖-接线表与引脚表.pdf`

## 9. License

个人毕业设计代码，仅供学习与交流参考。引用的 STM32 HAL 库版权归 STMicroelectronics 所有。
