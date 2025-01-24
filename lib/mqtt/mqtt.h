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
} MY_MQTT_DATA;

esp_err_t mqtt_init(MY_MQTT_DATA *my_mqtt_data);
void mqtt_app_start(void *pvParameter);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

#endif