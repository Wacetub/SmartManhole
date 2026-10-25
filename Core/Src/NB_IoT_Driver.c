#include "NB_IoT_Driver.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart2; 

static void nb_uart2_drain(uint32_t drain_ms) {
    uint32_t start = HAL_GetTick();
    uint8_t ch;
    while (HAL_GetTick() - start < drain_ms) {
        if (HAL_UART_Receive(&huart2, &ch, 1, 5) != HAL_OK) {
            break;
        }
    }
}

static int nb_uart2_read_response(char *out, int out_size, uint32_t total_timeout_ms, uint32_t idle_timeout_ms) {
    uint32_t start = HAL_GetTick();
    uint32_t last_rx = start;
    int len = 0;
    uint8_t ch;

    if (out_size <= 0) return 0;

    while ((HAL_GetTick() - start < total_timeout_ms) && (HAL_GetTick() - last_rx < idle_timeout_ms)) {
        if (HAL_UART_Receive(&huart2, &ch, 1, 10) == HAL_OK) {
            if (len < out_size - 1) {
                out[len++] = (char)ch;
            }
            last_rx = HAL_GetTick();
        }
    }
    out[len] = '\0';
    return len;
}

static int extract_json_float(const char *text, const char *key, float *out_val) {
    char pattern[64];
    char *pos;
    char *end_ptr;
    float val;
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    pos = strstr((char *)text, pattern);
    if (!pos) return 0;
    pos += strlen(pattern);
    val = strtof(pos, &end_ptr);
    if (end_ptr == pos) return 0;
    *out_val = val;
    return 1;
}

static int hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_to_bytes(const char *hex, uint8_t *out, int out_max) {
    int i = 0;
    int out_len = 0;
    while (hex[i] && hex[i + 1] && out_len < out_max) {
        int hi = hex_char_to_val(hex[i]);
        int lo = hex_char_to_val(hex[i + 1]);
        if (hi < 0 || lo < 0) break;
        out[out_len++] = (uint8_t)((hi << 4) | lo);
        i += 2;
    }
    return out_len;
}

static uint8_t parse_thresholds_from_mqtt_payload(const uint8_t *data, int len, float *tilt, float *water, float *gas) {
    char json_buf[512];
    int start = -1;
    int end = -1;
    int copy_len;
    uint8_t updated = 0;
    float v;

    if (len <= 0) return 0;

    // NSORF 读回的是 MQTT 二进制帧，不能直接按 C 字符串解析。
    // 先在原始字节流中提取 JSON 片段 {...}，再做键值解析。
    for (int i = 0; i < len; i++) {
        if (data[i] == '{') {
            start = i;
            break;
        }
    }
    for (int i = len - 1; i >= 0; i--) {
        if (data[i] == '}') {
            end = i;
            break;
        }
    }
    if (start < 0 || end < 0 || end <= start) return 0;

    copy_len = end - start + 1;
    if (copy_len >= (int)sizeof(json_buf)) {
        copy_len = (int)sizeof(json_buf) - 1;
    }
    memcpy(json_buf, &data[start], copy_len);
    json_buf[copy_len] = '\0';

    if (extract_json_float(json_buf, "threshold_tilt", &v) || extract_json_float(json_buf, "tilt", &v)) {
        *tilt = v;
        updated = 1;
    }
    if (extract_json_float(json_buf, "threshold_water", &v) || extract_json_float(json_buf, "water", &v)) {
        *water = v;
        updated = 1;
    }
    if (extract_json_float(json_buf, "threshold_gas", &v) || extract_json_float(json_buf, "gas", &v)) {
        *gas = v;
        updated = 1;
    }
    return updated;
}

static uint8_t parse_thresholds_from_text_fallback(const char *text, float *tilt, float *water, float *gas) {
    char buf[1024];
    int j = 0;
    float v;
    uint8_t updated = 0;

    // 去掉 \" 里的反斜杠，兼容 OneNET 命令以转义 JSON 下发的场景
    for (int i = 0; text[i] != '\0' && j < (int)sizeof(buf) - 1; i++) {
        if (text[i] == '\\' && text[i + 1] == '"') {
            continue;
        }
        buf[j++] = text[i];
    }
    buf[j] = '\0';

    if (extract_json_float(buf, "threshold_tilt", &v) || extract_json_float(buf, "tilt", &v)) {
        *tilt = v;
        updated = 1;
    }
    if (extract_json_float(buf, "threshold_water", &v) || extract_json_float(buf, "water", &v)) {
        *water = v;
        updated = 1;
    }
    if (extract_json_float(buf, "threshold_gas", &v) || extract_json_float(buf, "gas", &v)) {
        *gas = v;
        updated = 1;
    }
    return updated;
}

void NB_IoT_Init(void) {
    NB_IoT_Send_AT_Command("AT+RST");
    HAL_Delay(3000);
    
    NB_IoT_Send_AT_Command("AT+CPIN?");
    HAL_Delay(1000);
    
    NB_IoT_Send_AT_Command("AT+CGATT=1");
    HAL_Delay(3000);
    
    NB_IoT_Connect_OneNET_MQTT();
}

void NB_IoT_Connect_OneNET_MQTT(void) {
    char cmd_buffer[256];
    
    NB_IoT_Send_AT_Command("AT+NSOCL=0"); 
    HAL_Delay(1000);
    NB_IoT_Send_AT_Command("AT+NSOCL=1"); 
    HAL_Delay(1000);
    
    NB_IoT_Send_AT_Command("AT+NSOCR=STREAM,6,0,1");
    HAL_Delay(1000); 

    snprintf(cmd_buffer, sizeof(cmd_buffer), "AT+NSOCO=0,\"%s\",%d", ONENET_MQTT_SERVER, ONENET_MQTT_PORT);
    NB_IoT_Send_AT_Command(cmd_buffer);
    
    HAL_Delay(3000); 
    printf("[NB-IoT] TCP Connected to OneNET. Sending MQTT CONNECT Packet...\n");

    static uint8_t connect_packet[256];
    int pkt_len = 0;
    
    int cid_len = strlen(ONENET_DEV_ID);
    int usr_len = strlen(ONENET_PROJ_ID);
    int pwd_len = strlen(ONENET_AUTH_INFO);
    
    int remain_len = 10 + (2 + cid_len) + (2 + usr_len) + (2 + pwd_len);
    
    connect_packet[pkt_len++] = 0x10;
    
    do {
        uint8_t encoded_byte = remain_len % 128;
        remain_len = remain_len / 128;
        if (remain_len > 0) {
            encoded_byte |= 128;
        }
        connect_packet[pkt_len++] = encoded_byte;
    } while (remain_len > 0);
    
    connect_packet[pkt_len++] = 0x00;
    connect_packet[pkt_len++] = 0x04;
    connect_packet[pkt_len++] = 'M';
    connect_packet[pkt_len++] = 'Q';
    connect_packet[pkt_len++] = 'T';
    connect_packet[pkt_len++] = 'T';
    
    connect_packet[pkt_len++] = 0x04;
    connect_packet[pkt_len++] = 0xC2;
    connect_packet[pkt_len++] = 0x00;
    connect_packet[pkt_len++] = 0x78;
    
    connect_packet[pkt_len++] = (cid_len >> 8) & 0xFF;
    connect_packet[pkt_len++] = cid_len & 0xFF;
    memcpy(&connect_packet[pkt_len], ONENET_DEV_ID, cid_len);
    pkt_len += cid_len;
    
    connect_packet[pkt_len++] = (usr_len >> 8) & 0xFF;
    connect_packet[pkt_len++] = usr_len & 0xFF;
    memcpy(&connect_packet[pkt_len], ONENET_PROJ_ID, usr_len);
    pkt_len += usr_len;
    
    connect_packet[pkt_len++] = (pwd_len >> 8) & 0xFF;
    connect_packet[pkt_len++] = pwd_len & 0xFF;
    memcpy(&connect_packet[pkt_len], ONENET_AUTH_INFO, pwd_len);
    pkt_len += pwd_len;

    NB_IoT_Send_TCP_Data(connect_packet, pkt_len);
    
    HAL_Delay(3000);
    printf("[NB-IoT] OneNET MQTT CONNECT Packet Sent.\n");
    
    uint32_t wait_time = HAL_GetTick();
    uint8_t rx_buf[128];
    int rx_len = 0;
    while(HAL_GetTick() - wait_time < 3000) {
        uint8_t ch;
        if(HAL_UART_Receive(&huart2, &ch, 1, 10) == HAL_OK) {
            rx_buf[rx_len++] = ch;
            if(rx_len >= sizeof(rx_buf) - 1) break;
        }
    }
    
    if (rx_len > 0) {
        printf("[NB-IoT] OneNET CONNACK (Hex): ");
        for (int i = 0; i < rx_len; i++) {
            printf("%02X ", rx_buf[i]);
        }
        printf("\n");
    } else {
        printf("[NB-IoT] No CONNACK received.\n");
    }
}

void NB_IoT_Subscribe_Downlink(void) {
    uint8_t subscribe_packet[128];
    const char* topic = "$creq/#";
    int topic_len = strlen(topic);
    int pkt_len = 0;
    int remain_len;
    uint16_t packet_id = 1;

    remain_len = 2 + 2 + topic_len + 1;
    subscribe_packet[pkt_len++] = 0x82;
    subscribe_packet[pkt_len++] = (uint8_t)remain_len;
    subscribe_packet[pkt_len++] = (packet_id >> 8) & 0xFF;
    subscribe_packet[pkt_len++] = packet_id & 0xFF;
    subscribe_packet[pkt_len++] = (topic_len >> 8) & 0xFF;
    subscribe_packet[pkt_len++] = topic_len & 0xFF;
    memcpy(&subscribe_packet[pkt_len], topic, topic_len);
    pkt_len += topic_len;
    subscribe_packet[pkt_len++] = 0x00;

    printf("[NB-IoT] Subscribing downlink topic: %s\n", topic);
    NB_IoT_Send_TCP_Data(subscribe_packet, pkt_len);
}

uint8_t NB_IoT_Poll_Threshold_Command(float *tilt, float *water, float *gas) {
    char cmd[] = "AT+NSORF=0,512";
    char at_resp[1024];
    uint16_t at_len = 0;
    uint32_t start_time = HAL_GetTick();
    uint8_t ch;
    char *line_pos;
    int comma_count = 0;
    char *hex_start = NULL;
    char *hex_end = NULL;
    uint8_t mqtt_raw[512];
    int mqtt_len;

    HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 1000);
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 1000);

    while (HAL_GetTick() - start_time < 1500) {
        if (HAL_UART_Receive(&huart2, &ch, 1, 10) == HAL_OK) {
            if (at_len < sizeof(at_resp) - 1) {
                at_resp[at_len++] = (char)ch;
            }
        }
    }
    at_resp[at_len] = '\0';
    if (at_len == 0) return 0;

    line_pos = strstr(at_resp, "+NSORF:");
    if (!line_pos) {
        // 回退：有些场景模块直接返回可见文本，尝试直接抽取阈值
        if (parse_thresholds_from_text_fallback(at_resp, tilt, water, gas)) {
            printf("[NB-IoT] Threshold update from text fallback: tilt=%.1f water=%.1f gas=%.1f\n", *tilt, *water, *gas);
            return 1;
        }
        return 0;
    }

    while (*line_pos) {
        if (*line_pos == ',') {
            comma_count++;
            if (comma_count == 4) {
                hex_start = line_pos + 1;
                break;
            }
        }
        line_pos++;
    }
    if (!hex_start) return 0;

    hex_end = hex_start;
    while (*hex_end && *hex_end != ',' && *hex_end != '\r' && *hex_end != '\n') hex_end++;
    *hex_end = '\0';
    if (strlen(hex_start) < 2) return 0;

    mqtt_len = hex_to_bytes(hex_start, mqtt_raw, sizeof(mqtt_raw));
    if (mqtt_len <= 0) {
        if (parse_thresholds_from_text_fallback(at_resp, tilt, water, gas)) {
            printf("[NB-IoT] Threshold update from text fallback: tilt=%.1f water=%.1f gas=%.1f\n", *tilt, *water, *gas);
            return 1;
        }
        return 0;
    }

    if (parse_thresholds_from_mqtt_payload(mqtt_raw, mqtt_len, tilt, water, gas)) {
        printf("[NB-IoT] Threshold update from cloud: tilt=%.1f water=%.1f gas=%.1f\n", *tilt, *water, *gas);
        return 1;
    }

    // 二进制包内未提取成功时，回退尝试解析 AT 文本体（含转义 JSON）
    if (parse_thresholds_from_text_fallback(at_resp, tilt, water, gas)) {
        printf("[NB-IoT] Threshold update from text fallback: tilt=%.1f water=%.1f gas=%.1f\n", *tilt, *water, *gas);
        return 1;
    }
    return 0;
}

uint8_t NB_IoT_Send_TCP_Data(uint8_t *data, uint16_t len) {
    static char hex_str[1024]; 
    char rx_buf[256];
    int rx_len;
    if (len * 2 >= sizeof(hex_str)) return 0; // 防止越界
    
    for (int i = 0; i < len; i++) {
        sprintf(&hex_str[i * 2], "%02X", data[i]);
    }
    
    static char cmd[1100];
    snprintf(cmd, sizeof(cmd), "AT+NSOSD=0,%d,%s", len, hex_str);

    nb_uart2_drain(30);
    HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 1000);
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 1000);

    rx_len = nb_uart2_read_response(rx_buf, sizeof(rx_buf), 1200, 200);
    if (rx_len <= 0) {
        printf("[NB-IoT] NSOSD timeout.\n");
        return 0;
    }

    if (strstr(rx_buf, "ERROR") || strstr(rx_buf, "FAIL")) {
        printf("[NB-IoT] NSOSD error: %s\n", rx_buf);
        return 0;
    }

    if (strstr(rx_buf, "+NSOSD:") || strstr(rx_buf, "OK")) {
        return 1;
    }

    printf("[NB-IoT] NSOSD unknown resp: %s\n", rx_buf);
    return 0;
}

void NB_IoT_Send_AT_Command(const char* cmd) {
    char rx_buf[256];
    int rx_len;
    nb_uart2_drain(30);
    HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 1000);
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 1000);
    
    printf("[NB-IoT] Send: %s\n", cmd);
    rx_len = nb_uart2_read_response(rx_buf, sizeof(rx_buf), 1500, 250);
    
    if(rx_len > 0) {
        printf("[NB-IoT] Recv:\n%s", rx_buf);
    } else {
        printf("[NB-IoT] Recv: No Response!\n");
    }
}

uint8_t NB_IoT_Publish_Data(const char* payload) {
    static uint8_t mqtt_publish_packet[512];
    int pkt_len = 0;
    
    const char* topic = "$dp";
    int topic_len = strlen(topic);
    
    int json_len = strlen(payload);
    int payload_len = 1 + 2 + json_len;
    
    int remain_len = 2 + topic_len + payload_len;
    
    mqtt_publish_packet[pkt_len++] = 0x30;
    
    do {
        uint8_t encoded_byte = remain_len % 128;
        remain_len = remain_len / 128;
        if (remain_len > 0) {
            encoded_byte |= 128;
        }
        mqtt_publish_packet[pkt_len++] = encoded_byte;
    } while (remain_len > 0);
    
    mqtt_publish_packet[pkt_len++] = (topic_len >> 8) & 0xFF;
    mqtt_publish_packet[pkt_len++] = topic_len & 0xFF;
    memcpy(&mqtt_publish_packet[pkt_len], topic, topic_len);
    pkt_len += topic_len;
    
    mqtt_publish_packet[pkt_len++] = 0x03;
    mqtt_publish_packet[pkt_len++] = (json_len >> 8) & 0xFF;
    mqtt_publish_packet[pkt_len++] = json_len & 0xFF;
    memcpy(&mqtt_publish_packet[pkt_len], payload, json_len);
    pkt_len += json_len;

    return NB_IoT_Send_TCP_Data(mqtt_publish_packet, pkt_len);
}

void NB_IoT_Send_Ping(void) {
    uint8_t ping_packet[2] = {0xC0, 0x00};
    if (!NB_IoT_Send_TCP_Data(ping_packet, 2)) {
        printf("[NB-IoT] PINGREQ failed.\n");
    }
}
