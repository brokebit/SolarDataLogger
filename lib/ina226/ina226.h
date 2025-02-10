#ifndef MAIN_INA226_H_
#define MAIN_INA226_H_

#define INA226_CAL_VAL          0x4F2
#define INA226_CFG_REG_VAL      0x4527 // Average over 16 Samples
#define INA226_SLAVE_ADDRESS	0x40
#define INA226_CFG_REG		    0x00
#define INA226_SHUNT_VOLT_REG	0x01
#define INA226_BUS_VOLT_REG	    0x02
#define INA226_POWER_REG	    0x03
#define INA226_CURRENT_REG	    0x04
#define INA226_CAL_REG		    0x05
#define INA226_MASKEN_REG	    0x06
#define INA226_ALERT_LMT_REG	0x07
#define INA226_MANUFACTURER_ID	0xFE
#define INA226_UNIQ_MFG_ID      0x5449
#define INA226_DIE_ID		    0xFF


typedef struct {
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_handle_t bus_handle;
    float voltage;
    float current;
    float power;
} INA226_DATA;


esp_err_t ina226_init(INA226_DATA *ina226_data);
esp_err_t ina226_voltage(INA226_DATA *ina226_data);
esp_err_t ina226_current(INA226_DATA *ina226_data);
esp_err_t ina226_power(INA226_DATA *ina226_data);
esp_err_t ina226_write_reg(INA226_DATA *ina226_data, uint8_t reg, uint16_t data);
esp_err_t ina226_read_reg(INA226_DATA *ina226_data, uint8_t reg, uint16_t *data);

#endif