#include "mqtt_client.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <victron.h>
#include "mqtt.h"
#include "ota.h"


// QueueHandle_t xQueue_mqtt;

static const char *MQTTTAG = "Mqtt-Task";

esp_err_t mqtt_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data) {
    int msg_id;
    msg_id = esp_mqtt_client_publish(client, topic, data, 0, 1, 0);
    ESP_LOGI(MQTTTAG, "Sent publish successful, msg_id=%d", msg_id);
    return ESP_OK;
}

esp_err_t process_command(char *command, MY_MQTT_DATA *my_mqtt_data) {
    ESP_LOGI(MQTTTAG, "Processing command: %s", command);

    if (strncmp(command, "ota", 3) == 0) {
        ESP_LOGI(MQTTTAG, "OTA Command Recieved");
        run_ota(my_mqtt_data->mac);
    }   

    if (strncmp(command, "rbk", 3) == 0) {
        ESP_LOGI(MQTTTAG, "Rollback Command Recieved");
        run_rollback();
    }

    if (strncmp(command, "rst", 3) == 0) {
        ESP_LOGI(MQTTTAG, "Reset Command Recieved");
        esp_restart();
    }

    if (strncmp(command, "png", 3) == 0) {
        ESP_LOGI(MQTTTAG, "Ping Command Recieved");
        mqtt_publish(my_mqtt_data->client, my_mqtt_data->mac, "pong");
    }

    if (strncmp(command, "ver", 3) == 0) {
        ESP_LOGI(MQTTTAG, "Version Command Recieved");
        mqtt_publish(my_mqtt_data->client, my_mqtt_data->mac, my_mqtt_data->fmw_version);
    }

    if (strncmp(command, "mem", 3) == 0) {
        ESP_LOGI(MQTTTAG, "Memory Command Recieved");
        char mem_data[16];
        snprintf(mem_data, sizeof(mem_data), "%li", esp_get_free_heap_size());
        mqtt_publish(my_mqtt_data->client, my_mqtt_data->mac, mem_data);
    }

    if (strncmp(command, "tmp", 3) == 0) {
        ESP_LOGI(MQTTTAG, "Temperature Command Recieved");
        char temp_data[6];
        snprintf(temp_data, sizeof(temp_data), "%.3f", my_mqtt_data->aht20_data.temperature_c);
        mqtt_publish(my_mqtt_data->client, my_mqtt_data->mac, temp_data);
    }

    if (strncmp(command, "hum", 3) == 0) {
        ESP_LOGI(MQTTTAG, "Humidity Command Recieved");
        char hum_data[6];
        snprintf(hum_data, sizeof(hum_data), "%.3f", my_mqtt_data->aht20_data.humidity);
        mqtt_publish(my_mqtt_data->client, my_mqtt_data->mac, hum_data);
    }

    if (strncmp(command, "gp1-0", 5)) {
        ESP_LOGI("MQTTTAG", "Turn off GPIO1");
        gpio_set_level(1,0);
    }

    if (strncmp(command, "gp1-1", 5)) {
        ESP_LOGI("MQTTTAG", "Turn on GPIO1");
        gpio_set_level(1,1);
    }


    return ESP_OK;
}

esp_err_t mqtt_init(MY_MQTT_DATA *my_mqtt_data) {

    // skip server verification: This is an insecure option provided in the ESP-TLS for testing purposes. 
    // The option can be set by enabling CONFIG_ESP_TLS_INSECURE and CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY 
    // in the ESP-TLS menuconfig. When this option is enabled the ESP-TLS will skip server verification by 
    // default when no other options for server verification are selected in the esp_tls_cfg_t structure.
    // When the above are set you don't need set set .verification.certificate 
    // Otherwise set .verification.certificate = mqtt_root_ca_pem_start 
    // which is defined in mqtt.h

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = my_mqtt_data->broker_url,
        },
    };

    my_mqtt_data->client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(my_mqtt_data->client, ESP_EVENT_ANY_ID, mqtt_event_handler, my_mqtt_data);
    esp_mqtt_client_start(my_mqtt_data->client);
    
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
                snprintf(SolarData_Now.IL, sizeof(SolarData_Now.IL), "%.3f", my_mqtt_data->ina226_data.current);
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

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {

    MY_MQTT_DATA *my_mqtt_data = (MY_MQTT_DATA *)handler_args;

    ESP_LOGD(MQTTTAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    // esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_CONNECTED");
        char cmd_channel[17]; 
        snprintf(cmd_channel, sizeof(cmd_channel), "%s-cmd", my_mqtt_data->mac);
        ESP_LOGI(MQTTTAG, "Subscribing to command channel: %s", cmd_channel);
        int msg_id = esp_mqtt_client_subscribe(my_mqtt_data->client, cmd_channel, 1);
        ESP_LOGI(MQTTTAG, "sent subscribe successful, msg_id=%d", msg_id);
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
        process_command(event->data, my_mqtt_data);
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