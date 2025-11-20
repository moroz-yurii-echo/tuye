#include "datapoints.h"

const dp_descriptor_t g_dp_descriptors[] = {
    {DP_INV_STATUS, "inv_status", "state"},
    {DP_GRID_VOLTAGE, "grid_voltage", "V"},
    {DP_GRID_FREQUENCY, "grid_frequency", "Hz"},
    {DP_ACTIVE_POWER, "active_power", "W"},
    {DP_DAILY_YIELD, "daily_yield", "kWh"},
    {DP_TOTAL_YIELD, "total_yield", "kWh"},
    {DP_PV1_VOLTAGE, "pv1_voltage", "V"},
    {DP_PV1_CURRENT, "pv1_current", "A"},
    {DP_PV2_VOLTAGE, "pv2_voltage", "V"},
    {DP_PV2_CURRENT, "pv2_current", "A"},
    {DP_HEATSINK_TEMP, "heatsink_temp", "°C"},
    {DP_LOGGER_RSSI, "logger_rssi", "dBm"},
};

const size_t g_dp_descriptor_count = sizeof(g_dp_descriptors) / sizeof(g_dp_descriptors[0]);
