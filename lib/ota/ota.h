#ifndef OTA_H
#define OTA_H

#include "esp_http_client.h"

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_pem_end");
void run_rollback();
esp_err_t client_event_handler(esp_http_client_event_t *evt);
void run_ota(char *mac);

#endif