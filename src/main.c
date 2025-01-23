
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "driver/i2c_master.h"

#include "esp_ws28xx.h"

#define LED_GPIO 8
CRGB* ws2812_buffer;


#if (INA226_PRESENT == 1) || (AHT20_PRESENT == 1)
    #include "my-i2c.h"
    i2c_master_bus_handle_t i2c_bus_handle;

#endif

#if INA226_PRESENT == 1
    #include "ina226.h"
    INA226_DATA ina226_data;
#endif 

#if AHT20_PRESENT == 1
    #include "aht20.h"
    AHT20_DATA aht20_data;
#endif

#define ESP_MAXIMUM_RETRY  3

#define CONFIG_BROKER_URL "mqtt://192.168.100.20"

#include <victron.h>
#include "mqtt_client.h"
#include "secrets.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Interval in which to publish MQTT Data in seconds (This is approximate due to processing time and such)
#define MQTT_PUBLISH_INTERVAL 60
#define MQTT_TOPIC         "sensors_beta"
#define QUEUE_REFILL_WAIT 3000

QueueHandle_t xQueue_mqtt;

static const char *TAG = "Main-Task";
static const char *WIFITAG = "Wifi-Tasks";
static const char *MQTTTAG = "Mqtt-Tasks";

static int s_retry_num = 0;

static void mqtt_app_start(void *pvParameter)
{
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)pvParameter;

    // xEventGroupWaitBits(my_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);

    const char *mqtt_data;
    struct SolarData SolarData_Now;
    char influx_string[256];
    mqtt_data = &influx_string[0];
    int influxString_length = 0;
    int publish_interval_counter = 0;
    

    while (1) {
        
        if (xQueueReceive(xQueue_mqtt, &(SolarData_Now),60000/portTICK_PERIOD_MS) != pdTRUE) {

            ESP_LOGE(MQTTTAG,"fail to recieve queued value");

        } else {
            
            if (AHT20_PRESENT == 1) {
                aht20_trigger_measurement(&aht20_data);
                snprintf(SolarData_Now.TEMP, sizeof(SolarData_Now.TEMP), "%.2f", aht20_data.temperature_c);
                snprintf(SolarData_Now.HUMD, sizeof(SolarData_Now.HUMD), "%.2f", aht20_data.humidity);
            }

            if (INA226_PRESENT == 1) {
                ina226_voltage(&ina226_data);
                // snprintf(SolarData_Now.V, sizeof(SolarData_Now.V), "%f", ina226_data.voltage);
                ina226_current(&ina226_data);
                snprintf(SolarData_Now.IL, sizeof(SolarData_Now.IL), "%.2f", ina226_data.current);
                ina226_power(&ina226_data);
                // snprintf(SolarData_Now.PPV, sizeof(SolarData_Now.PPV), "%f", ina226_data.power);
            }

            sprintf(influx_string,"sensor,location=%s,type=esp32c6,id=%s panel_voltage=%s,battery_voltage=%s,load_out_state=%s,battery_current=%s,mem_alloc=%i,mem_free=%i,load_current=%s,temp=%s,humidity=%s", \
                                   "alley", SolarData_Now.SERIAL, SolarData_Now.VPV, SolarData_Now.V, SolarData_Now.LOAD, SolarData_Now.I, \
                                   (int)SolarData_Now.free_mem_data, (int)SolarData_Now.free_mem_data, SolarData_Now.IL, SolarData_Now.TEMP, SolarData_Now.HUMD);
            
            influxString_length = strlen(influx_string);

            if (publish_interval_counter == MQTT_PUBLISH_INTERVAL) {
                esp_mqtt_client_publish(client, MQTT_TOPIC, mqtt_data, influxString_length, 0, 0);
                publish_interval_counter = 0; 
            } else {
                publish_interval_counter++;
            }

        }

        if (uxQueueMessagesWaiting(xQueue_mqtt) == 0 ) {
            vTaskDelay(QUEUE_REFILL_WAIT/portTICK_PERIOD_MS);
            ESP_LOGD(MQTTTAG,"Holding...");
        }

        vTaskDelay(1000/portTICK_PERIOD_MS);
        ESP_LOGD(MQTTTAG,"Looping %d", publish_interval_counter);
    }
    
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTTTAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_CONNECTED");
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        //ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTTTAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            // log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            // log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            // log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGD(MQTTTAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(MQTTTAG, "Other event id:%d", event->event_id);
        break;
    }
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGE(WIFITAG, "Retrying to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGE(WIFITAG,"connect to the AP fail");
        esp_restart();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFITAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER, */
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(WIFITAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFITAG, "connected to ap SSID:%s password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFITAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(WIFITAG, "UNEXPECTED EVENT");
    }
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_init(LED_GPIO, WS2812B, 1, &ws2812_buffer));
    ws2812_buffer[0] = (CRGB){.r=25, .g=8, .b=0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_update());

    vTaskDelay(2000 / portTICK_PERIOD_MS); // debug delay

    #if (INA226_PRESENT == 1) || (AHT20_PRESENT == 1)
        ESP_LOGI(TAG, "At least one of INA226 or AHT20 is present and I2C driver is included.");
        my_i2c_init(I2C_CONTROLLER_0, 19, 18, &i2c_bus_handle);
        
    #endif
    
    #if (INA226_PRESENT == 1)
        ina226_data.bus_handle = i2c_bus_handle;
        ina226_init(&ina226_data);
	#endif

    #if (AHT20_PRESENT == 1)
        aht20_data.bus_handle = i2c_bus_handle;
        aht20_init(&aht20_data);
    #endif

    ESP_LOGI(TAG, "Initializing Wifi");
    wifi_init_sta();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, "WootWoot", 0, 1, 0);

    xQueue_mqtt = xQueueCreate( 2, sizeof( struct SolarData));
    if (xQueue_mqtt == NULL) {
        ESP_LOGE(TAG, "Error creating the queue");
        return;
    }

    xTaskCreate(recieve_serial, "recieve_serial", 4096, (void *)xQueue_mqtt, 1, NULL);
    //xTaskCreate(recieve_serial, "recieve_serial", 4096, &recieve_serial_args, 1, NULL);
    xTaskCreate(mqtt_app_start, "mqtt_app_start", 4096, (void *)client, 1, NULL);

    msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, "WootWoot 22222", 0, 1, 0);

    ws2812_buffer[0] = (CRGB){.r=0, .g=5, .b=0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_update());

}
