# ESP32 TuyaOpen bridge for Deye Sun-6K (Solarman stick logger)

This project sketches the firmware side needed to send real-time telemetry from a Deye SUN-6K inverter to the Tuya cloud using an ESP32 and the **TuyaOpen** SDK. Instead of connecting to the inverter over RS485/Modbus, the device polls the Solarman stick logger over its TCP/Modbus bridge and forwards the resulting datapoints to Tuya.

## Layout

- `src/main.c` – glue code that boots the Tuya SDK, polls the logger and pushes datapoints.
- `src/solarman_stick_client.c` – lightweight Modbus/TCP client tuned for the Solarman stick logger register map.
- `src/datapoints.c` / `include/datapoints.h` – central place for datapoint IDs that match the Tuya portal definition.
- `include/solarman_stick_client.h` – public API for the logger client.

## Configuration

1. Create the device in the Tuya IoT platform and note the **Product ID**, **UUID** and **AuthKey**.
2. Open `src/main.c` and set:
   - `TUYA_PRODUCT_ID`, `TUYA_UUID`, `TUYA_AUTH_KEY` – credentials from your Tuya project.
   - `LOGGER_HOST`/`LOGGER_PORT` – IP/port of the Solarman stick (defaults to 8899).
   - `LOGGER_STICK_SN` – serial number printed on the stick (used only for logging, but helps when multiple sticks are on the LAN).
   - `LOGGER_MODBUS_ID` – inverter Modbus unit ID behind the stick.
3. Ensure the datapoint IDs in `include/datapoints.h` match the IDs defined on the Tuya portal. The names in `g_dp_descriptors` are only for clarity.

## Build and flash

The code is written for ESP-IDF with the Tuya Open SDK linked in your project. A typical build looks like:

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

You need to link the Tuya Open SDK (TuyaOS) libraries that provide the `tuya_open_sdk_*` symbols declared in `src/main.c` and the ESP-IDF components such as `esp_http_client` and `cJSON`.

## Runtime flow

1. `app_main` initializes the TCP client for the Solarman stick logger and boots the Tuya stack.
2. `logger_poll_task` runs every 5 seconds:
   - Fetches a block of holding registers from the logger via Modbus/TCP.
   - Maps the registers to inverter values using the Deye register addresses defined in `src/solarman_stick_client.c`.
   - Reports each value to the Tuya cloud using the data point IDs already configured in the Tuya portal.
3. Logs show current grid voltage/frequency, output power and PV string data for quick debugging.

Adapt the polling interval or fields as needed to match your logger firmware or to add extra datapoints (e.g., battery SOC on hybrid models).
