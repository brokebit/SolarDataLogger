#include <victron.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include "driver/uart.h"
#include <esp_err.h>
#include "esp_log.h"
#include <freertos/queue.h>
#include "my-i2c.h"
#include "ina226.h"

static const char *VICTRONTAG = "solar_victron";

void recieve_serial(void *pvParameter)
{
    ESP_LOGI(VICTRONTAG, "recieve_serial task started");

    // Recieve_Serial_Args *recieve_serial_args = (Recieve_Serial_Args *)pvParameter;

    QueueHandle_t xQueue_mqtt = (QueueHandle_t)pvParameter;
    char serial_received_characters[SERIAL_BUFFER_SIZE];
    struct SolarDataElement SolarDataElement_Now;
    struct SolarData SolarData_Now;

    /* strcpy(SolarData_Now.FW, "*");
    strcpy(SolarData_Now.SERIAL, "*");
    strcpy(SolarData_Now.V, "*");
    strcpy(SolarData_Now.I, "*");
    strcpy(SolarData_Now.VPV, "*");
    strcpy(SolarData_Now.PPV, "*");
    strcpy(SolarData_Now.CS, "*");
    strcpy(SolarData_Now.MPPT, "*");
    strcpy(SolarData_Now.ERR, "*");
    strcpy(SolarData_Now.LOAD, "*");
    strcpy(SolarData_Now.IL, "*");
    strcpy(SolarData_Now.H19, "*");
    strcpy(SolarData_Now.H20, "*");
    strcpy(SolarData_Now.H21, "*");
    strcpy(SolarData_Now.H22, "*");
    strcpy(SolarData_Now.H23, "*");
    strcpy(SolarData_Now.HSDS, "*");*/
    memset(&SolarData_Now, '*', sizeof(SolarData_Now));
    SolarData_Now.free_mem_data = 0;

    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, VICTRON_SERIAL_TXD, VICTRON_SERIAL_RXD, VICTRON_SERIAL_RTS, VICTRON_SERIAL_CTS));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0));

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);

    if (data == NULL)
    {
        ESP_LOGE(VICTRONTAG, "Failed to allocate memory for data");
        vTaskDelete(NULL);
        return;
    }
    static int ndx = 0;

    while (1)
    {
        // Read data from the UART
        int len = uart_read_bytes(UART_NUM_1, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len < 0)
        {
            ESP_LOGE(VICTRONTAG, "Error reading data from UART");
            vTaskDelete(NULL);
            return;
        }
        // ESP_LOGE(VICTRONTAG, "Data received 1: %s", data);
        //printf("%s \r", data);

        for (int x = 0; x <= len; x++)
        {

            if (data[x] != '\n')
            {
                if (data[x] != '\r')
                {
                    serial_received_characters[ndx] = data[x];
                    ndx++;
                    // printf("%c", received_characters[ndx]);
                    //ESP_LOGI(VICTRONTAG, "Data received 2: %s", serial_received_characters);
            
                    if (ndx >= SERIAL_BUFFER_SIZE)
                    {
                        ndx = SERIAL_BUFFER_SIZE - 1;
                    }
                }
            }
            else
            {
                serial_received_characters[ndx] = '\0';
                // printf("SERIAL_DATA: %s",serial_received_characters);
                // ESP_LOGI(VICTRONTAG, "SERIAL_DATA 3: %s", serial_received_characters);

                char *piece = strtok(serial_received_characters, "\t");
                int pieceIndex = 0;
                while (piece != NULL)
                {

                    if (pieceIndex == 0)
                    {
                        strcpy(SolarDataElement_Now.name, piece);
                    }
                    else if (pieceIndex == 1)
                    {
                        strcpy(SolarDataElement_Now.value, piece);
                        // strncpy(SolarDataElement_Now.value, piece, strlen(piece)-1);
                    }
                    pieceIndex++;
                    piece = strtok(NULL, "\t");
                }
                ndx = 0;
                // ESP_LOGV(VICTRONTAG, "SERIALDATA 4:%s: %s", SolarDataElement_Now.name, SolarDataElement_Now.value);

                if (strcmp(SolarDataElement_Now.name, "Checksum") == 0)
                {

                    SolarData_Now.free_mem_data = esp_get_free_heap_size();
                    // add free memeory, time stamp and send to queue for sending via MQTT

                    xQueueSend(xQueue_mqtt, &SolarData_Now, (TickType_t)0);
                }
                else
                {
                    if (strcmp(SolarDataElement_Now.name, "FW") == 0)
                    {
                        strlcpy(SolarData_Now.FW, SolarDataElement_Now.value, sizeof(SolarData_Now.FW));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "SER#") == 0)
                    {
                        strlcpy(SolarData_Now.SERIAL, SolarDataElement_Now.value, sizeof(SolarData_Now.SERIAL));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "V") == 0)
                    {
                        strlcpy(SolarData_Now.V, SolarDataElement_Now.value, sizeof(SolarData_Now.V));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "I") == 0)
                    {
                        strlcpy(SolarData_Now.I, SolarDataElement_Now.value, sizeof(SolarData_Now.I));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "VPV") == 0)
                    {
                        strlcpy(SolarData_Now.VPV, SolarDataElement_Now.value, sizeof(SolarData_Now.VPV));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "PPV") == 0)
                    {
                        strlcpy(SolarData_Now.PPV, SolarDataElement_Now.value, sizeof(SolarData_Now.PPV));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "CS") == 0)
                    {
                        strlcpy(SolarData_Now.CS, SolarDataElement_Now.value, sizeof(SolarData_Now.CS));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "MPPT") == 0)
                    {
                        strlcpy(SolarData_Now.MPPT, SolarDataElement_Now.value, sizeof(SolarData_Now.MPPT));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "ERR") == 0)
                    {
                        strlcpy(SolarData_Now.ERR, SolarDataElement_Now.value, sizeof(SolarData_Now.ERR));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "LOAD") == 0)
                    {
                        //printf("LOAD: %s\n", SolarDataElement_Now.value);
                        if (strcmp(SolarDataElement_Now.value, "ON") == 0)
                        {
                            strcpy(SolarData_Now.LOAD, "1");
                        }
                        else
                        {
                            strcpy(SolarData_Now.LOAD, "0");
                        }
                        //strlcpy(SolarData_Now.LOAD, SolarDataElement_Now.value, sizeof(SolarData_Now.LOAD));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "IL") == 0)
                    {
                        strlcpy(SolarData_Now.IL, SolarDataElement_Now.value, sizeof(SolarData_Now.IL));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "H19") == 0)
                    {
                        strlcpy(SolarData_Now.H19, SolarDataElement_Now.value, sizeof(SolarData_Now.H19));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "H20") == 0)
                    {
                        strlcpy(SolarData_Now.H20, SolarDataElement_Now.value, sizeof(SolarData_Now.H20));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "H21") == 0)
                    {
                        strlcpy(SolarData_Now.H21, SolarDataElement_Now.value, sizeof(SolarData_Now.H21));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "H22") == 0)
                    {
                        strlcpy(SolarData_Now.H22, SolarDataElement_Now.value, sizeof(SolarData_Now.H22));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "H23") == 0)
                    {
                        strlcpy(SolarData_Now.H23, SolarDataElement_Now.value, sizeof(SolarData_Now.H23));
                    }
                    else if (strcmp(SolarDataElement_Now.name, "HSDS") == 0)
                    {
                        strlcpy(SolarData_Now.HSDS, SolarDataElement_Now.value, sizeof(SolarData_Now.HSDS));
                    }
                }
            }
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    free(data);
}