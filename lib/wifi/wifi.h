#ifndef WIFI_H
#define WIFI_H

#include <esp_event.h>

#define SERVICE_NAME "PROV_123456"
#define SERVICE_KEY "abcd1234"
#define POP "abcd12345"
#define ESP_MAXIMUM_RETRY  3

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void wifi_init();
static void event_handler(void* handler_args, esp_event_base_t event_base, int32_t event_id, void* event_data);

#endif