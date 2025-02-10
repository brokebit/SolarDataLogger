#include <stdio.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_event.h"
#include "esp_log.h"
#include "my-i2c.h"
#include "ina226.h"

static const char *INA226TAG = "ina226-driver";

esp_err_t ina226_init(INA226_DATA *ina226_data)
{
	ESP_LOGI(INA226TAG, "Firmware includes INA226 driver.");
	esp_err_t ret = ESP_OK;

	i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA226_SLAVE_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = ESP_OK;

    //i2c_master_dev_handle_t dev_handle;
    err = i2c_master_bus_add_device(ina226_data->bus_handle, &dev_cfg, &ina226_data->dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(INA226TAG, "Error adding device to bus");
        return err;
    }

	ret = ina226_write_reg(ina226_data, INA226_CFG_REG, 0x8000);

	if (ret != ESP_OK) {
		ESP_LOGE(INA226TAG, "INA226 init failed.");
		return(ret);

	}

	ret = ina226_write_reg(ina226_data, INA226_CAL_REG, INA226_CAL_VAL);

	if (ret != ESP_OK) {
		ESP_LOGE(INA226TAG, "Failed to write to configuration register");
		return(ret);

	}

	ret = ina226_write_reg(ina226_data, INA226_CAL_REG, INA226_CAL_VAL);

	if (ret != ESP_OK) {
		ESP_LOGE(INA226TAG, "Failed to write to calibration register");
		return(ret);

	}

	uint16_t mfgId = 0;
	ina226_read_reg(ina226_data, INA226_MANUFACTURER_ID, &mfgId);

    if (mfgId != INA226_UNIQ_MFG_ID) {
		ESP_LOGE(INA226TAG, "Failed to find the INA226\n");
		return ESP_FAIL;
	}

	uint16_t dieId = 0;
	ina226_read_reg(ina226_data, INA226_DIE_ID, &dieId);
	
	uint16_t cfgReg = 0;
	ina226_read_reg(ina226_data, INA226_CFG_REG, &cfgReg);

	ESP_LOGI(INA226TAG,"Manufacturer ID:        0x%04X\r\n", mfgId);
	ESP_LOGI(INA226TAG,"Die ID Register:        0x%04X\r\n", dieId);
	ESP_LOGI(INA226TAG,"Configuration Register: 0x%04X\r\n", cfgReg);
	ESP_LOGI("INA226", "INA226 init successful");

	return (ret);

}

esp_err_t ina226_voltage(INA226_DATA *ina226_data) {

	uint16_t voltReg = 0;
	esp_err_t err = ESP_OK;

	err = ina226_read_reg(ina226_data, INA226_BUS_VOLT_REG, &voltReg);

	if (err != ESP_OK) {
		ESP_LOGE(INA226TAG, "Failed to read from bus voltage register");
		return err;
	}

	ina226_data->voltage = (voltReg) * 0.00125;
	return err;
}

esp_err_t ina226_current(INA226_DATA *ina226_data) {

	uint16_t currentReg = 0;
	esp_err_t err = ESP_OK;

	err = ina226_read_reg(ina226_data, INA226_CURRENT_REG, &currentReg);

	if (err != ESP_OK) {
		ESP_LOGE(INA226TAG, "Failed to read from current register");
		return err;
	}

	// Internally Calculated as Current = ((ShuntVoltage * CalibrationRegister) / 2048)
	// supposedly by casting currentReg into a int16_t the twos compliment value will be delt with. 

    ina226_data->current = (int16_t)currentReg*.0005;

	return err;
}

esp_err_t ina226_power(INA226_DATA *ina226_data) {

	uint16_t powerReg = 0;
	esp_err_t err = ESP_OK;

	err = ina226_read_reg(ina226_data, INA226_POWER_REG, &powerReg);

	if (err != ESP_OK) {
		ESP_LOGE(INA226TAG, "Failed to read from power register");
		return err;
	}

	// The Power Register LSB is internally programmed to equal 25 times the programmed value of the Current_LSB
	ina226_data->power = powerReg * 0.0125;

	return err;
}

esp_err_t ina226_write_reg(INA226_DATA *ina226_data, uint8_t reg, uint16_t data) {

    if (!ina226_data || !ina226_data->dev_handle) {
		ESP_LOGE(INA226TAG, "Error: Invalid INA226 data or device handle");
        return ESP_ERR_INVALID_ARG;
    }

	uint8_t cmd_data[3] = {reg, (data & 0xFF00) >> 8, data & 0xFF};

	return i2c_master_transmit(ina226_data->dev_handle, cmd_data, 3, 1000);

}

esp_err_t ina226_read_reg(INA226_DATA *ina226_data, uint8_t reg, uint16_t *data) {
	uint8_t reg_data[2] = {0, 0};

	esp_err_t ret = ESP_OK;
	ret = i2c_master_transmit_receive(ina226_data->dev_handle, &reg, 1, reg_data, 2, 1000);

	*data = ((uint16_t)reg_data[0] << 8) | (uint16_t)reg_data[1];
	return ret;
}
