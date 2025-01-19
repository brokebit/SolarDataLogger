#ifndef _AHT20_H_
#define _AHT20_H_

// See section 5.3 of datasheet
#define AHT20_ADDR 0x38

// Command bytes. See section 5.3 and 5.4 of datasheet
#define AHT20_CMD_INIT 0xBE
#define AHT20_INIT_PARAM1 0x08
#define AHT20_INIT_PARAM2 0x00
#define AHT20_INIT_DELAY_MS 40 
#define AHT20_CAL_DELAY_MS 10 

#define AHT20_CMD_RESET 0xBA

#define AHT20_CMD_MEASURE 0xAC
#define AHT20_MEASURE_PARAM1 0x33
#define AHT20_MEASURE_PARAM2 0x00
#define AHT20_MEASURE_DELAY_MS 80

#define AHT20_REG_STATUS 0x71

typedef struct {
    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_handle_t bus_handle;
    float temperature_c;
    float temperature_f;
    float humidity;
} AHT20_DATA;

esp_err_t aht20_init(AHT20_DATA *aht20_data);
esp_err_t aht20_trigger_measurement(AHT20_DATA *aht20_data);
esp_err_t aht20_read_reg_byte(AHT20_DATA *aht20_data, uint8_t reg, uint8_t *data);
esp_err_t aht20_read_bytes(AHT20_DATA *aht20_data, uint8_t *data, uint8_t size);
esp_err_t aht20_write_byte(AHT20_DATA *aht20_data, uint8_t *data);
esp_err_t aht20_write_bytes(AHT20_DATA *aht20_data, uint8_t *data, uint8_t size);

#endif