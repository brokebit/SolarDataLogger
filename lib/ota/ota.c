#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "ota.h"

static const char *OTATAG = "OTA-Task";

esp_err_t client_event_handler(esp_http_client_event_t *evt) {
  return ESP_OK;
}

void run_rollback() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *old = esp_ota_get_next_update_partition(running);
    esp_ota_set_boot_partition(old);
    esp_restart();
}

void run_ota(char *mac) {

    ESP_LOGI(OTATAG, "Invoking OTA");

    //const char *url = "https://brokebit-esp32-firmware.s3.us-east-1.amazonaws.com/";
    size_t full_url_length = strlen(OTA_BASE_URL) + strlen(mac) + 1;
    char *full_url = (char *)malloc(full_url_length);
    snprintf(full_url, full_url_length, "%s%s", OTA_BASE_URL, mac);
    ESP_LOGI(OTATAG, "Full URL: %s\n", full_url);

    esp_http_client_config_t clientConfig = {
        .url = full_url,
        .event_handler = client_event_handler,
        .cert_pem = (char *)aws_root_ca_pem_start};

    // IDF V5
    esp_https_ota_config_t ota_config = {
        .http_config = &clientConfig};

    if (esp_https_ota(&ota_config) == ESP_OK)
    {
      ESP_LOGI(OTATAG, "OTA flash succsessfull for version.");
      ESP_LOGE(OTATAG, "Restarting in 5 seconds\n");
      vTaskDelay(pdMS_TO_TICKS(5000));
      esp_restart();
    }
    ESP_LOGE(OTATAG, "Failed to update firmware");
    free(full_url);
}