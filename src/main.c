
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

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

#include "wifi.h"

#include "driver/i2c_master.h"
#include "mqtt.h"
#include "ota.h"
#include "esp_ws28xx.h"

#define FIRMWARE_VERSION "1.1.4"
#define SENSOR_LOCATION "alley"
#define SENSOR_TYPE "esp32c6"

// #define GPIO1_CTL CONFIG_GPIO_INPUT_1
// #define GPIO2_CTL CONFIG_GPIO_INPUT_2

#define LED_GPIO 8
#define SDA_GPIO 18
#define SCL_GPIO 19

// Interval in which to publish MQTT Data in seconds (This is approximate due to processing time and such)
#define MQTT_PUBLISH_INTERVAL 60
#define MQTT_TOPIC         "sensors_beta"
#define QUEUE_REFILL_WAIT 3000
#define CONFIG_BROKER_URL "mqtt://192.168.100.20"

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

#include <victron.h>
#include "secrets.h"

CRGB* ws2812_buffer;
static const char *TAG = "Main-Task";
static char my_mac[13];
static EventGroupHandle_t s_wifi_event_group;

void app_main(void) {

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_init(LED_GPIO, WS2812B, 1, &ws2812_buffer));
    ws2812_buffer[0] = (CRGB){.r=25, .g=8, .b=0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_update());

    vTaskDelay(2000 / portTICK_PERIOD_MS); // debug delay

    uint8_t mac[6];
    esp_base_mac_addr_get(mac);
    snprintf(my_mac, 13, "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI("TAG", "MAC Address: %s", my_mac);

    MY_MQTT_DATA *my_mqtt_data = malloc(sizeof(MY_MQTT_DATA));
    

    my_mqtt_data->broker_url = CONFIG_BROKER_URL;
    my_mqtt_data->topic = MQTT_TOPIC;
    my_mqtt_data->sensor_location = SENSOR_LOCATION;
    my_mqtt_data->sensor_type = SENSOR_TYPE;
    my_mqtt_data->publish_interval = MQTT_PUBLISH_INTERVAL;
    my_mqtt_data->queue_refill_wait = QUEUE_REFILL_WAIT;
    my_mqtt_data->xQueue_mqtt = xQueueCreate( 2, sizeof( struct SolarData));
    my_mqtt_data->mac = my_mac;
    my_mqtt_data->fmw_version = FIRMWARE_VERSION;

    #if (INA226_PRESENT == 1) || (AHT20_PRESENT == 1)
        ESP_LOGI(TAG, "At least one of INA226 or AHT20 is present and I2C driver is included.");
        my_i2c_init(I2C_CONTROLLER_0, 19, 18, &i2c_bus_handle);
    #endif
    
    #if (INA226_PRESENT == 1)
        ina226_data.bus_handle = i2c_bus_handle;
        ina226_init(&ina226_data);
        my_mqtt_data->ina226_data = ina226_data;
	#endif

    #if (AHT20_PRESENT == 1)
        aht20_data.bus_handle = i2c_bus_handle;
        aht20_init(&aht20_data);
        my_mqtt_data->aht20_data = aht20_data;
    #endif

    ESP_LOGI(TAG, "Initializing Wifi");
    wifi_init(s_wifi_event_group);
    
    if (my_mqtt_data->xQueue_mqtt == NULL) { 
        ESP_LOGE(TAG, "Error creating the queue");
        return;
    }

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    mqtt_init(my_mqtt_data);

    xTaskCreate(recieve_serial, "recieve_serial", 4096, (void *)my_mqtt_data->xQueue_mqtt, 1, NULL);
    xTaskCreate(mqtt_app_start, "mqtt_app_start", 4096, (void *)my_mqtt_data, 1, NULL);

    mqtt_publish(my_mqtt_data->client, my_mac, FIRMWARE_VERSION);

    ws2812_buffer[0] = (CRGB){.r=0, .g=5, .b=0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(ws28xx_update());

    ESP_LOGI(TAG, "Firmware Version: %s", FIRMWARE_VERSION);

    gpio_dump_io_configuration(stdout, (1ULL << 1) |(1ULL << 2));

    gpio_config_t gpio_cfg = {};
    gpio_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_cfg.mode = GPIO_MODE_OUTPUT;
    gpio_cfg.pin_bit_mask = ((1ULL << 1)|(1ULL << 2));
    gpio_cfg.pull_down_en = 1;
    gpio_cfg.pull_up_en = 0;
    gpio_config(&gpio_cfg);
    
    gpio_dump_io_configuration(stdout, (1ULL << 1) |(1ULL << 2));
    
}
