#include "solarman_stick_client.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esp_log.h"

#define MODBUS_FUNCTION_READ_HOLDING_REGISTERS 0x03

// Default block of registers we read in a single request. Adjust the addresses
// to match the Solarman/Deye mapping you use in production.
#define REG_BLOCK_START       0x0200
#define REG_BLOCK_COUNT       0x0058

#define REG_INV_STATUS        0x0200
#define REG_GRID_VOLTAGE      0x021E
#define REG_GRID_FREQUENCY    0x021F
#define REG_ACTIVE_POWER      0x0221
#define REG_DAILY_YIELD       0x023A
#define REG_TOTAL_YIELD       0x023B
#define REG_PV1_VOLTAGE       0x0201
#define REG_PV1_CURRENT       0x0202
#define REG_PV2_VOLTAGE       0x0203
#define REG_PV2_CURRENT       0x0204
#define REG_HEATSINK_TEMP     0x0246
#define REG_LOGGER_SIGNAL     0x0257

static const char *TAG = "solarman";
static char s_host[64];
static uint16_t s_port;
static uint32_t s_stick_sn;
static uint8_t s_modbus_id;
static uint16_t s_transaction_id = 0x01;

static bool connect_socket(int *out_fd) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket alloc failed");
        return false;
    }

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(s_port),
    };

    if (inet_pton(AF_INET, s_host, &dest.sin_addr.s_addr) != 1) {
        ESP_LOGE(TAG, "invalid Solarman host");
        close(sock);
        return false;
    }

    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
        ESP_LOGE(TAG, "connect to %s:%u failed", s_host, s_port);
        close(sock);
        return false;
    }

    *out_fd = sock;
    return true;
}

static uint16_t read_u16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static bool read_register_block(uint16_t start, uint16_t count, uint16_t *out) {
    uint8_t request[12];
    uint16_t tid = s_transaction_id++;

    request[0] = tid >> 8;
    request[1] = tid & 0xFF;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = 0x00;
    request[5] = 0x06; // length
    request[6] = s_modbus_id;
    request[7] = MODBUS_FUNCTION_READ_HOLDING_REGISTERS;
    request[8] = start >> 8;
    request[9] = start & 0xFF;
    request[10] = count >> 8;
    request[11] = count & 0xFF;

    int fd;
    if (!connect_socket(&fd)) {
        return false;
    }

    ssize_t sent = send(fd, request, sizeof(request), 0);
    if (sent != sizeof(request)) {
        ESP_LOGE(TAG, "failed to send request");
        close(fd);
        return false;
    }

    // Expect MBAP (7 bytes) + func + byte_count + payload
    uint8_t response[260];
    ssize_t received = recv(fd, response, sizeof(response), 0);
    close(fd);

    if (received < 9) {
        ESP_LOGE(TAG, "short response (%d bytes)", (int)received);
        return false;
    }

    uint8_t function_code = response[7];
    if (function_code != MODBUS_FUNCTION_READ_HOLDING_REGISTERS) {
        ESP_LOGE(TAG, "unexpected function code 0x%02X", function_code);
        return false;
    }

    uint8_t byte_count = response[8];
    if (byte_count != count * 2 || received < 9 + byte_count) {
        ESP_LOGE(TAG, "unexpected payload size %u (wanted %u)", byte_count, count * 2);
        return false;
    }

    const uint8_t *payload = &response[9];
    for (uint16_t i = 0; i < count; ++i) {
        out[i] = read_u16(payload + i * 2);
    }

    return true;
}

static bool sample_from_registers(const uint16_t *block, inverter_sample_t *out_sample) {
    if (!out_sample) {
        return false;
    }

    if (REG_BLOCK_START + REG_BLOCK_COUNT < REG_LOGGER_SIGNAL) {
        ESP_LOGE(TAG, "register block is too small for mapping");
        return false;
    }

    uint16_t reg_value;

    reg_value = (REG_INV_STATUS < REG_BLOCK_START ||
                 REG_INV_STATUS >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_INV_STATUS - REG_BLOCK_START];
    out_sample->inverter_on = reg_value != 0;

    reg_value = (REG_GRID_VOLTAGE < REG_BLOCK_START ||
                 REG_GRID_VOLTAGE >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_GRID_VOLTAGE - REG_BLOCK_START];
    out_sample->grid_voltage = reg_value / 10.0f;

    reg_value = (REG_GRID_FREQUENCY < REG_BLOCK_START ||
                 REG_GRID_FREQUENCY >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_GRID_FREQUENCY - REG_BLOCK_START];
    out_sample->grid_frequency = reg_value / 100.0f;

    reg_value = (REG_ACTIVE_POWER < REG_BLOCK_START ||
                 REG_ACTIVE_POWER >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_ACTIVE_POWER - REG_BLOCK_START];
    out_sample->active_power = (float)((int16_t)reg_value);

    reg_value = (REG_DAILY_YIELD < REG_BLOCK_START ||
                 REG_DAILY_YIELD >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_DAILY_YIELD - REG_BLOCK_START];
    out_sample->daily_yield = reg_value / 10.0f;

    reg_value = (REG_TOTAL_YIELD < REG_BLOCK_START ||
                 REG_TOTAL_YIELD >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_TOTAL_YIELD - REG_BLOCK_START];
    out_sample->total_yield = reg_value / 10.0f;

    reg_value = (REG_PV1_VOLTAGE < REG_BLOCK_START ||
                 REG_PV1_VOLTAGE >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_PV1_VOLTAGE - REG_BLOCK_START];
    out_sample->pv1_voltage = reg_value / 10.0f;

    reg_value = (REG_PV1_CURRENT < REG_BLOCK_START ||
                 REG_PV1_CURRENT >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_PV1_CURRENT - REG_BLOCK_START];
    out_sample->pv1_current = reg_value / 10.0f;

    reg_value = (REG_PV2_VOLTAGE < REG_BLOCK_START ||
                 REG_PV2_VOLTAGE >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_PV2_VOLTAGE - REG_BLOCK_START];
    out_sample->pv2_voltage = reg_value / 10.0f;

    reg_value = (REG_PV2_CURRENT < REG_BLOCK_START ||
                 REG_PV2_CURRENT >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_PV2_CURRENT - REG_BLOCK_START];
    out_sample->pv2_current = reg_value / 10.0f;

    reg_value = (REG_HEATSINK_TEMP < REG_BLOCK_START ||
                 REG_HEATSINK_TEMP >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_HEATSINK_TEMP - REG_BLOCK_START];
    out_sample->heatsink_temp = (float)((int16_t)reg_value) / 10.0f;

    reg_value = (REG_LOGGER_SIGNAL < REG_BLOCK_START ||
                 REG_LOGGER_SIGNAL >= REG_BLOCK_START + REG_BLOCK_COUNT)
                    ? 0
                    : block[REG_LOGGER_SIGNAL - REG_BLOCK_START];
    out_sample->logger_rssi = (int16_t)reg_value;

    return true;
}

bool solarman_stick_client_init(const char *host, uint16_t port, uint32_t stick_sn, uint8_t modbus_id) {
    if (!host || strlen(host) >= sizeof(s_host)) {
        ESP_LOGE(TAG, "invalid host");
        return false;
    }

    snprintf(s_host, sizeof(s_host), "%s", host);
    s_port = port;
    s_stick_sn = stick_sn;
    s_modbus_id = modbus_id;

    ESP_LOGI(TAG, "solarman stick host=%s port=%u sn=%u unit_id=%u", s_host, s_port, (unsigned)s_stick_sn, s_modbus_id);
    return true;
}

bool solarman_stick_client_fetch(inverter_sample_t *out_sample) {
    if (!out_sample) {
        return false;
    }

    uint16_t registers[REG_BLOCK_COUNT] = {0};
    if (!read_register_block(REG_BLOCK_START, REG_BLOCK_COUNT, registers)) {
        return false;
    }

    return sample_from_registers(registers, out_sample);
}
