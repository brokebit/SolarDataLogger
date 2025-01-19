#ifndef VICTRON_H
#define VICTRON_H
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "driver/gpio.h"

#define VICTRON_SERIAL_TXD  (GPIO_NUM_11)
#define VICTRON_SERIAL_RXD  (GPIO_NUM_10)
#define VICTRON_SERIAL_RTS  (UART_PIN_NO_CHANGE)
#define VICTRON_SERIAL_CTS  (UART_PIN_NO_CHANGE)
#define BUF_SIZE (1024)
#define SERIAL_BUFFER_SIZE 32
#define UART_BAUD_RATE 19200 

struct SolarDataElement
{
    char name[16];
    char value[16];
};

// Structure to hold victron data
struct SolarData
{
    char FW[16];
    char SERIAL[16];
    char V[6];
    char I[6];
    char VPV[6];
    char PPV[16];
    char CS[16];
    char MPPT[16];
    char ERR[16];
    char LOAD[2];
    char IL[6];
    char H19[16];
    char H20[16];
    char H21[16];
    char H22[16];
    char H23[16];
    char HSDS[16];
    char TEMP[16];
    char HUMD[16];
    uint32_t free_mem_data;
};

void recieve_serial(void *pvParameter);

#endif
