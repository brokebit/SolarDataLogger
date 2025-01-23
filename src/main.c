
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
#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"


#include "driver/i2c_master.h"
#include "mqtt.h"
#include "ota.h"
#include "esp_ws28xx.h"

#define FIRMWARE_VERSION "1.0.0"

#define SENSOR_LOCATION "alley"
#define SENSOR_TYPE "esp32c6"

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

static const char *TAG = "Main-Task";
static const char *WIFITAG = "Wifi-Task";

static int s_retry_num = 0;
static char my_mac[13];

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
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

void wifi_init_sta(void) {
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

void app_main(void) {
    uint8_t mac[6];
    esp_base_mac_addr_get(mac);
    snprintf(my_mac, 13, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI("TAG", "MAC Address: %s", my_mac);

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


    MY_MQTT_DATA *my_mqtt_data = malloc(sizeof(MY_MQTT_DATA));
    

    my_mqtt_data->broker_url = CONFIG_BROKER_URL;
    my_mqtt_data->topic = MQTT_TOPIC;
    my_mqtt_data->sensor_location = SENSOR_LOCATION;
    my_mqtt_data->sensor_type = SENSOR_TYPE;
    my_mqtt_data->publish_interval = MQTT_PUBLISH_INTERVAL;
    my_mqtt_data->queue_refill_wait = QUEUE_REFILL_WAIT;
    my_mqtt_data->xQueue_mqtt = xQueueCreate( 2, sizeof( struct SolarData));
    
    if (my_mqtt_data->xQueue_mqtt == NULL) { 
        ESP_LOGE(TAG, "Error creating the queue");
        return;
    }

    mqtt_init(my_mqtt_data);

    xTaskCreate(recieve_serial, "recieve_serial", 4096, (void *)my_mqtt_data->xQueue_mqtt, 1, NULL);
    xTaskCreate(mqtt_app_start, "mqtt_app_start", 4096, (void *)my_mqtt_data, 1, NULL);

    esp_mqtt_client_publish(my_mqtt_data->client, my_mac, FIRMWARE_VERSION, 0, 1, 0);

    ws2812_buffer[0] = (CRGB){.r=0, .g=5, .b=0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_update());

}
