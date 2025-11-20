#ifndef WIFI_LOGGER_CLIENT_H
#define WIFI_LOGGER_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include "datapoints.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool inverter_on;
    float grid_voltage;
    float grid_frequency;
    float active_power;
    float daily_yield;
    float total_yield;
    float pv1_voltage;
    float pv1_current;
    float pv2_voltage;
    float pv2_current;
    float heatsink_temp;
    int logger_rssi;
} inverter_sample_t;

// Initializes the HTTP client used to query the Wi-Fi logger.
bool wifi_logger_client_init(const char *base_url, const char *auth_token);

// Queries the logger and populates the inverter_sample_t struct.
bool wifi_logger_client_fetch(inverter_sample_t *out_sample);

#ifdef __cplusplus
}
#endif

#endif // WIFI_LOGGER_CLIENT_H
