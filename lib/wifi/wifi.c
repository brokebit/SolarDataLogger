#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <wifi_provisioning/manager.h>

#include <wifi_provisioning/scheme_ble.h>

#include "wifi.h"

static const char *WIFITAG = "Wifi-Tasks";

// static EventGroupHandle_t s_wifi_event_group;

void wifi_init(EventGroupHandle_t *s_wifi_event_group) {

    
    // s_wifi_event_group = xEventGroupCreate();

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        ESP_LOGE(WIFITAG, "esp_event_loop_create_default failed");
        return;
    }

    err = esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
    err = esp_event_handler_instance_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
    err = esp_event_handler_instance_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, s_wifi_event_group, NULL);
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, s_wifi_event_group, NULL);
    
    if (err != ESP_OK) {
        ESP_LOGE(WIFITAG, "esp_event_handler_instance_register failed");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
    
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    
    if (!provisioned) {
        ESP_LOGI(WIFITAG, "Starting provisioning");

        const char *service_name = SERVICE_NAME;
        const char *service_key = SERVICE_KEY;

        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        const char *pop = POP;

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));

    } else {
        ESP_LOGI(WIFITAG, "Already provisioned, starting Wi-Fi STA");
        ESP_ERROR_CHECK(esp_wifi_start());
    }  
}

static void event_handler(void* handler_args, esp_event_base_t event_base, int32_t event_id, void* event_data) {
      
    EventGroupHandle_t s_wifi_event_group = (EventGroupHandle_t)handler_args;

    static int s_retry_num = 0;

    if (event_base == WIFI_PROV_EVENT) {
        switch(event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(WIFITAG, "Provisioning started");
                break; 
            case WIFI_PROV_CRED_RECV:
                ESP_LOGI(WIFITAG, "Credentials received");
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(WIFITAG, "SSID: %s", (char *)wifi_sta_cfg->ssid);
                ESP_LOGI(WIFITAG, "Password: %s", (char *)wifi_sta_cfg->password);
                break;
            case WIFI_PROV_CRED_FAIL:
                ESP_LOGE(WIFITAG, "Provisioning failed");
                break;
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(WIFITAG, "Provisioning successful");
                break;
            case WIFI_PROV_END:
                ESP_LOGI(WIFITAG, "Provisioning end");
                break;
            default:
                ESP_LOGW(WIFITAG, "Unhandled event %d", (int8_t)event_id);
        }
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGE(WIFITAG, "Retrying to connect to the AP");
        } else {
            // xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            printf("Holding here\n");
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