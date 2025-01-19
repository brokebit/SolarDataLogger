#include <freertos/FreeRTOS.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "my-i2c.h"
#include "aht20.h"

static const char *AHT20TAG = "aht20-driver";

esp_err_t aht20_init(AHT20_DATA *aht20_data) {

    ESP_LOGI(AHT20TAG, "Firmware includes AHT20 driver.");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AHT20_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = ESP_OK;

    //i2c_master_dev_handle_t dev_handle;
    err = i2c_master_bus_add_device(aht20_data->bus_handle, &dev_cfg, &aht20_data->dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(AHT20TAG, "Error adding device to bus");
        return err;
    }

    //Wait initial power up delay time as described in sectin 5.4 of datasheet
    vTaskDelay(AHT20_INIT_DELAY_MS / portTICK_PERIOD_MS);

    uint8_t cmd_data[3] = {AHT20_CMD_INIT, AHT20_INIT_PARAM1, AHT20_INIT_PARAM2};
    err = aht20_write_bytes(aht20_data, cmd_data, sizeof(cmd_data));
    if (err != ESP_OK) {
        ESP_LOGE(AHT20TAG, "Error initializing the device");
        return err;
    }

    vTaskDelay(AHT20_CAL_DELAY_MS / portTICK_PERIOD_MS);

    uint8_t data = 0;
    err = aht20_read_reg_byte(aht20_data, AHT20_REG_STATUS, &data);
    if (err != ESP_OK) {
        ESP_LOGE(AHT20TAG, "Error reading status from device");
        return err;
    }

    return ESP_OK;

}

esp_err_t aht20_trigger_measurement(AHT20_DATA *aht20_data) {

    esp_err_t err = ESP_OK;
    uint8_t cmd_data[3] = {AHT20_CMD_MEASURE, AHT20_MEASURE_PARAM1, AHT20_MEASURE_PARAM2};
    err = aht20_write_bytes(aht20_data, cmd_data, sizeof(cmd_data));
    if (err != ESP_OK) {
        ESP_LOGE(AHT20TAG, "Error writing to device");
        return err;
    }

    vTaskDelay(AHT20_MEASURE_DELAY_MS / portTICK_PERIOD_MS);

    uint8_t status_data = 0;
    err = aht20_read_reg_byte(aht20_data, AHT20_REG_STATUS, &status_data);
    if (err != ESP_OK) {
        ESP_LOGE(AHT20TAG, "Error reading status from device");
        return err;
    }
    
    //If Bit 7 is 0 then the measurement is ready
    if ((status_data & 0x80) != 0) {
        ESP_LOGE(AHT20TAG, "Measurement not ready");
        return ESP_FAIL;
    } 

    uint8_t raw_data[6] = {0,0,0,0,0,0};

    err = aht20_read_bytes(aht20_data, raw_data, 6);
    if (err != ESP_OK) {
        ESP_LOGE(AHT20TAG, "Error reading measurement data from device");
        return err;
    }

    //Shift the raw data to the correct position based on section 5.4 of datasheet
    uint32_t raw_humidity = ((uint32_t)raw_data[1] << 12) | ((uint32_t)raw_data[2] << 4) | ((uint32_t)raw_data[3] >> 4);
    uint32_t raw_temp = ((uint32_t)(raw_data[3] & 0x0F) << 16) | ((uint32_t)raw_data[4] << 8) | (uint32_t)raw_data[5];

    // Convert raw values to actual values based on section 6 of datasheet
    aht20_data->humidity = (raw_humidity * 100) / 1048576.0; // 2^20 = 1048576
    aht20_data->temperature_c = (raw_temp * 200) / 1048576.0 - 50; // 2^20 = 1048576
    aht20_data->temperature_f = aht20_data->temperature_c * 1.8 + 32;

    return ESP_OK;

}

esp_err_t aht20_read_reg_byte(AHT20_DATA *aht20_data, uint8_t reg, uint8_t *data) {

    return i2c_master_transmit_receive(aht20_data->dev_handle, &reg, 1, data, 1, 1000);

}

esp_err_t aht20_read_bytes(AHT20_DATA *aht20_data, uint8_t *data, uint8_t size) {
    
    return i2c_master_receive(aht20_data->dev_handle, data, size, 1000);

}

esp_err_t aht20_write_byte(AHT20_DATA *aht20_data, uint8_t *data) {

    return i2c_master_transmit(aht20_data->dev_handle, data, 1, 1000);

}
esp_err_t aht20_write_bytes(AHT20_DATA *aht20_data, uint8_t *data, uint8_t size) {

   return i2c_master_transmit(aht20_data->dev_handle, data, size, 1000);

}
