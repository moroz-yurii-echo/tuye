#ifndef SOLARMAN_STICK_CLIENT_H
#define SOLARMAN_STICK_CLIENT_H

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

// Initializes the TCP client used to query the Solarman stick logger.
bool solarman_stick_client_init(const char *host, uint16_t port, uint32_t stick_sn, uint8_t modbus_id);

// Queries the logger and populates the inverter_sample_t struct.
bool solarman_stick_client_fetch(inverter_sample_t *out_sample);

#ifdef __cplusplus
}
#endif

#endif // SOLARMAN_STICK_CLIENT_H
