#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "datapoints.h"
#include "solarman_stick_client.h"

// These credentials come from the Tuya IoT platform project created for the device.
#define TUYA_PRODUCT_ID     "your_product_id"
#define TUYA_UUID           "your_device_uuid"
#define TUYA_AUTH_KEY       "your_auth_key"
#define TUYA_FW_VERSION     "1.0.0"

// Solarman stick logger configuration.
#define LOGGER_HOST         "192.168.1.50"  // IP of the Solarman stick logger
#define LOGGER_PORT         8899             // TCP port used by the stick
#define LOGGER_STICK_SN     12345678         // optional stick serial number (informational only)
#define LOGGER_MODBUS_ID    1                // inverter Modbus unit ID behind the stick

static const char *TAG = "app";

// Minimal TuyaOpen SDK hooks. The actual implementation is provided by Tuya's IoT SDK
// and linked during compilation.
extern int tuya_open_sdk_init(const char *product_id,
                              const char *uuid,
                              const char *auth_key,
                              const char *firmware_version);

extern int tuya_open_sdk_report_bool(dp_id_t dp_id, bool value);
extern int tuya_open_sdk_report_float(dp_id_t dp_id, float value);
extern int tuya_open_sdk_report_int(dp_id_t dp_id, int value);

static void report_sample_to_tuya(const inverter_sample_t *sample) {
    tuya_open_sdk_report_bool(DP_INV_STATUS, sample->inverter_on);
    tuya_open_sdk_report_float(DP_GRID_VOLTAGE, sample->grid_voltage);
    tuya_open_sdk_report_float(DP_GRID_FREQUENCY, sample->grid_frequency);
    tuya_open_sdk_report_float(DP_ACTIVE_POWER, sample->active_power);
    tuya_open_sdk_report_float(DP_DAILY_YIELD, sample->daily_yield);
    tuya_open_sdk_report_float(DP_TOTAL_YIELD, sample->total_yield);
    tuya_open_sdk_report_float(DP_PV1_VOLTAGE, sample->pv1_voltage);
    tuya_open_sdk_report_float(DP_PV1_CURRENT, sample->pv1_current);
    tuya_open_sdk_report_float(DP_PV2_VOLTAGE, sample->pv2_voltage);
    tuya_open_sdk_report_float(DP_PV2_CURRENT, sample->pv2_current);
    tuya_open_sdk_report_float(DP_HEATSINK_TEMP, sample->heatsink_temp);
    tuya_open_sdk_report_int(DP_LOGGER_RSSI, sample->logger_rssi);
}

static void logger_poll_task(void *pv) {
    inverter_sample_t sample;

    while (1) {
        if (solarman_stick_client_fetch(&sample)) {
            ESP_LOGI(TAG, "Grid %.1f V %.1f Hz, AC %.0f W, PV1 %.1f/%.1f, PV2 %.1f/%.1f",
                     sample.grid_voltage, sample.grid_frequency, sample.active_power,
                     sample.pv1_voltage, sample.pv1_current,
                     sample.pv2_voltage, sample.pv2_current);
            report_sample_to_tuya(&sample);
        } else {
            ESP_LOGW(TAG, "Failed to read logger");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting Tuya + Deye bridge");

    if (!solarman_stick_client_init(LOGGER_HOST, LOGGER_PORT, LOGGER_STICK_SN, LOGGER_MODBUS_ID)) {
        ESP_LOGE(TAG, "Solarman logger init failed, rebooting");
        esp_restart();
    }

    if (tuya_open_sdk_init(TUYA_PRODUCT_ID, TUYA_UUID, TUYA_AUTH_KEY, TUYA_FW_VERSION) != 0) {
        ESP_LOGE(TAG, "Tuya init failed, rebooting");
        esp_restart();
    }

    xTaskCreate(logger_poll_task, "logger_poll", 4096, NULL, 5, NULL);
}
