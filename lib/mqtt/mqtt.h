#ifndef MQTT_H
#define MQTT_H

#include "mqtt_client.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#include "aht20.h"
#include "ina226.h"

typedef struct {
    esp_mqtt_client_handle_t client;
    QueueHandle_t xQueue_mqtt;
    char *broker_url;
    char *topic;
    AHT20_DATA aht20_data;
    INA226_DATA ina226_data;
    char *sensor_location;
    char *sensor_type;
    int publish_interval;
    int queue_refill_wait;
    char *mac;
    char *fmw_version;
} MY_MQTT_DATA;

// skip server verification: This is an insecure option provided in the ESP-TLS for testing purposes. 
// The option can be set by enabling CONFIG_ESP_TLS_INSECURE and CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY 
// in the ESP-TLS menuconfig. When this option is enabled the ESP-TLS will skip server verification by 
// default when no other options for server verification are selected in the esp_tls_cfg_t structure.
// When the above are set, the following lines are ignored.

extern const uint8_t mqtt_root_ca_pem_start[] asm("_binary_aws_root_pem_start");
extern const uint8_t mqtt_root_ca_pem_end[] asm("_binary_aws_root_pem_end");

esp_err_t mqtt_init(MY_MQTT_DATA *my_mqtt_data);
esp_err_t mqtt_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data);
void mqtt_app_start(void *pvParameter);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

#endif