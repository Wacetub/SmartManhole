#include "GPS_Driver.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// 内部变量
static GPS_Data current_gps = {0.0, 0.0, 0};
static char gps_buffer[128];
static uint8_t gps_buffer_index = 0;

void GPS_Init(void) {
    current_gps.longitude = 0.0;
    current_gps.latitude = 0.0;
    current_gps.is_valid = 0;
}

void Parse_GNRMC(char *line) {
    char *token;
    int field_count = 0;
    char lat_hemi = 'N';
    char lon_hemi = 'E';
    double parsed_lat = 0.0;
    double parsed_lon = 0.0;

    token = strtok(line, ",");
    while (token != NULL) {
        field_count++;

        if (field_count == 3) {
            if (strcmp(token, "A") == 0) {
                current_gps.is_valid = 1;
            } else {
                current_gps.is_valid = 0;
                return; // 无效定位，直接退出
            }
        }

        if (field_count == 4 && current_gps.is_valid) {
            double raw_lat = atof(token);
            int degrees = (int)(raw_lat / 100);
            double minutes = raw_lat - (degrees * 100);
            parsed_lat = degrees + (minutes / 60.0);
        }

        if (field_count == 5 && current_gps.is_valid && token[0] != '\0') {
            lat_hemi = token[0];
        }

        if (field_count == 6 && current_gps.is_valid) {
            double raw_lon = atof(token);
            int degrees = (int)(raw_lon / 100);
            double minutes = raw_lon - (degrees * 100);
            parsed_lon = degrees + (minutes / 60.0);
        }

        if (field_count == 7 && current_gps.is_valid && token[0] != '\0') {
            lon_hemi = token[0];
        }
        
        token = strtok(NULL, ",");
    }

    if (current_gps.is_valid) {
        current_gps.latitude = (lat_hemi == 'S') ? -parsed_lat : parsed_lat;
        current_gps.longitude = (lon_hemi == 'W') ? -parsed_lon : parsed_lon;
    }
}

void GPS_Process_Data(uint8_t byte) {
    if (byte == '$') {
        gps_buffer_index = 0;
    }
    
    if (gps_buffer_index < sizeof(gps_buffer) - 1) {
        gps_buffer[gps_buffer_index++] = byte;
    }
    
    if (byte == '\n') {
        gps_buffer[gps_buffer_index] = '\0';
        
        if (strstr(gps_buffer, "GNRMC") || strstr(gps_buffer, "GPRMC")) {
            Parse_GNRMC(gps_buffer);
        }
    }
}

void GPS_Get_Location(GPS_Data *data) {
    *data = current_gps;
}
