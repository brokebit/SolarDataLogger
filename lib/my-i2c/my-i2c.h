#ifndef _AH20_H_
#define _AH20_H_

#include "driver/i2c_master.h"

#define I2C_CONTROLLER_0		0
#define I2C_CONTROLLER_1		1

#define I2C_MASTER_FREQ_HZ		100000

esp_err_t my_i2c_init(uint8_t port, uint8_t sda, uint8_t scl, i2c_master_bus_handle_t *bus_handle);

#endif