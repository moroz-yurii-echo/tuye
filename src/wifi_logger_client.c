#include "wifi_logger_client.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "wifi_logger";
static char s_base_url[96];
static char s_auth_header[128];

bool wifi_logger_client_init(const char *base_url, const char *auth_token) {
    if (!base_url || strlen(base_url) >= sizeof(s_base_url)) {
        ESP_LOGE(TAG, "invalid base url");
        return false;
    }
    snprintf(s_base_url, sizeof(s_base_url), "%s", base_url);

    if (auth_token && auth_token[0] != '\0') {
        snprintf(s_auth_header, sizeof(s_auth_header), "Authorization: Bearer %s", auth_token);
    } else {
        s_auth_header[0] = '\0';
    }

    return true;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

static bool parse_response(const char *payload, inverter_sample_t *out_sample) {
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return false;
    }

    // Field names follow the Deye Wi-Fi logger JSON structure (firmware V1.65+).
    out_sample->inverter_on = cJSON_IsTrue(cJSON_GetObjectItem(root, "inverter_status"));
    out_sample->grid_voltage = cJSON_GetObjectItem(root, "grid_voltage")->valuedouble;
    out_sample->grid_frequency = cJSON_GetObjectItem(root, "grid_frequency")->valuedouble;
    out_sample->active_power = cJSON_GetObjectItem(root, "active_power")->valuedouble;
    out_sample->daily_yield = cJSON_GetObjectItem(root, "daily_yield")->valuedouble;
    out_sample->total_yield = cJSON_GetObjectItem(root, "total_yield")->valuedouble;
    out_sample->pv1_voltage = cJSON_GetObjectItem(root, "pv1_voltage")->valuedouble;
    out_sample->pv1_current = cJSON_GetObjectItem(root, "pv1_current")->valuedouble;
    out_sample->pv2_voltage = cJSON_GetObjectItem(root, "pv2_voltage")->valuedouble;
    out_sample->pv2_current = cJSON_GetObjectItem(root, "pv2_current")->valuedouble;
    out_sample->heatsink_temp = cJSON_GetObjectItem(root, "heatsink_temp")->valuedouble;
    out_sample->logger_rssi = cJSON_GetObjectItem(root, "wifi_rssi")->valueint;

    cJSON_Delete(root);
    return true;
}

bool wifi_logger_client_fetch(inverter_sample_t *out_sample) {
    if (!out_sample) {
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url), "%s/status", s_base_url);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 3500,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "failed to init http client");
        return false;
    }

    if (s_auth_header[0] != '\0') {
        esp_http_client_set_header(client, "Authorization", s_auth_header + strlen("Authorization: "));
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "unexpected status %d", status_code);
        esp_http_client_cleanup(client);
        return false;
    }

    int content_length = esp_http_client_get_content_length(client);
    char *buffer = calloc(1, content_length + 1);
    if (!buffer) {
        esp_http_client_cleanup(client);
        return false;
    }

    int read_len = esp_http_client_read_response(client, buffer, content_length);
    bool parsed = (read_len > 0) && parse_response(buffer, out_sample);
    free(buffer);
    esp_http_client_cleanup(client);
    return parsed;
}
