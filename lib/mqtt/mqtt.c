#include "mqtt_client.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <victron.h>
#include "mqtt.h"


// QueueHandle_t xQueue_mqtt;

static const char *MQTTTAG = "Mqtt-Task";

esp_err_t mqtt_init(MY_MQTT_DATA *my_mqtt_data) {

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = my_mqtt_data->broker_url,
    };

    my_mqtt_data->client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(my_mqtt_data->client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(my_mqtt_data->client);

    // int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, "WootWoot", 0, 1, 0);

    return ESP_OK;

}

void mqtt_app_start(void *pvParameter) {
    //esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)pvParameter;

    MY_MQTT_DATA *my_mqtt_data = (MY_MQTT_DATA *)pvParameter;

    const char *mqtt_data;
    struct SolarData SolarData_Now;
    char influx_string[256];
    mqtt_data = &influx_string[0];
    int influxString_length = 0;
    int publish_interval_counter = 0;
    

    while (1) {
        
        if (xQueueReceive(my_mqtt_data->xQueue_mqtt, &(SolarData_Now),60000/portTICK_PERIOD_MS) != pdTRUE) {

            ESP_LOGE(MQTTTAG,"fail to recieve queued value");

        } else {
            
            if (AHT20_PRESENT == 1) {
                aht20_trigger_measurement(&my_mqtt_data->aht20_data);
                snprintf(SolarData_Now.TEMP, sizeof(SolarData_Now.TEMP), "%.2f", my_mqtt_data->aht20_data.temperature_c);
                snprintf(SolarData_Now.HUMD, sizeof(SolarData_Now.HUMD), "%.2f", my_mqtt_data->aht20_data.humidity);
            }

            if (INA226_PRESENT == 1) {
                ina226_voltage(&my_mqtt_data->ina226_data);
                ina226_current(&my_mqtt_data->ina226_data);
                snprintf(SolarData_Now.IL, sizeof(SolarData_Now.IL), "%.2f", my_mqtt_data->ina226_data.current);
                ina226_power(&my_mqtt_data->ina226_data);
            }

            sprintf(influx_string,"sensor,location=%s,type=%s,id=%s panel_voltage=%s,battery_voltage=%s,load_out_state=%s,battery_current=%s,mem_alloc=%i,mem_free=%i,load_current=%s,temp=%s,humidity=%s", \
                                   my_mqtt_data->sensor_location, my_mqtt_data->sensor_type, SolarData_Now.SERIAL, SolarData_Now.VPV, SolarData_Now.V, SolarData_Now.LOAD, SolarData_Now.I, \
                                   (int)SolarData_Now.free_mem_data, (int)SolarData_Now.free_mem_data, SolarData_Now.IL, SolarData_Now.TEMP, SolarData_Now.HUMD);
            
            influxString_length = strlen(influx_string);

            if (publish_interval_counter == my_mqtt_data->publish_interval) {
                esp_mqtt_client_publish(my_mqtt_data->client, my_mqtt_data->topic, mqtt_data, influxString_length, 0, 0);
                publish_interval_counter = 0; 
            } else {
                publish_interval_counter++;
            }

        }

        if (uxQueueMessagesWaiting(my_mqtt_data->xQueue_mqtt) == 0 ) {
            vTaskDelay(my_mqtt_data->queue_refill_wait/portTICK_PERIOD_MS);
            ESP_LOGD(MQTTTAG,"Holding...");
        }

        vTaskDelay(1000/portTICK_PERIOD_MS);
        ESP_LOGD(MQTTTAG,"Looping %d", publish_interval_counter);
    }
    
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTTTAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    // esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_DATA");
        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGD(MQTTTAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));            
        }
        break;
    default:
        ESP_LOGI(MQTTTAG, "Other event id:%d", event->event_id);
        break;
    }
}