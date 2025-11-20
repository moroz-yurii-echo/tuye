#ifndef DATAPOINTS_H
#define DATAPOINTS_H

#include <stdint.h>
#include <stdbool.h>

// Logical data points defined in the Tuya portal.
typedef enum {
    DP_INV_STATUS = 1,
    DP_GRID_VOLTAGE = 2,
    DP_GRID_FREQUENCY = 3,
    DP_ACTIVE_POWER = 4,
    DP_DAILY_YIELD = 5,
    DP_TOTAL_YIELD = 6,
    DP_PV1_VOLTAGE = 7,
    DP_PV1_CURRENT = 8,
    DP_PV2_VOLTAGE = 9,
    DP_PV2_CURRENT = 10,
    DP_HEATSINK_TEMP = 11,
    DP_LOGGER_RSSI = 12
} dp_id_t;

// Helper structure used when publishing multiple data points.
typedef struct {
    dp_id_t id;
    const char *key;
    const char *units;
} dp_descriptor_t;

extern const dp_descriptor_t g_dp_descriptors[];
extern const size_t g_dp_descriptor_count;

#endif // DATAPOINTS_H
