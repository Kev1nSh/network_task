#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>

void serial_task(void *pvParameters);

#endif

#include "driver/uart.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "printer_helper.h"
#include "string.h"
#include "shared_resources.h"
#include "mqtt_client.h"

#define UART_NUM UART_NUM_0
#define BUF_SIZE 256
#define TX_PIN 16
#define RX_PIN 17

#define MAX_MSG_LEN 128

void serial_task(void *pvParameters)
{
    PRINTFC_SERIAL("Serial task started");

    uint8_t data[BUF_SIZE];
    uart_event_t event;

    // UART configuration
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    // Install UART driver
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &serial_msg_queue, 0);

    PRINTFC_SERIAL("UART driver installed");

    while (1)
    {
        // Wait for UART event
        if (xQueueReceive(serial_msg_queue, (void *)&event, portMAX_DELAY) == pdTRUE)
        {
            switch (event.type)
            {
                case UART_DATA:
                    memset(data, 0, sizeof(data));
                    uart_read_bytes(UART_NUM, data, event.size, pdTICKS_TO_MS(500));
                    data[event.size] = '\0'; // Ensure null-termination
                    PRINTFC_SERIAL("Received %d bytes", event.size);
                    PRINTFC_SERIAL("Data: %s", data);
                    
                    int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
                    if (event.size > 0) {
                        data[event.size] = '\0';  // Null terminate the string

                        // Process commands from serial input
                        PRINTFC_SERIAL("Received command: %s", data);

                        // Handle choice commands:
                        //      "/r " = http request
                        //      "/v " = mqtt choice
                        //      "/s " = mqtt say
                        if (strstr((char *)data, "/v ok")) {
                            PRINTFC_SERIAL("Choice: OK");
                            esp_mqtt_client_publish(mqtt_client, topic_player_uplink, "{\"val\": \"ok\"}", 0, 1, 0);
                        } else if (strstr((char *)data, "/v neka")) {
                            PRINTFC_SERIAL("Choice: NEKA");
                            esp_mqtt_client_publish(mqtt_client, topic_player_uplink, "{\"val\": \"neka\"}", 0, 1, 0);
                        } else if (strstr((char *)data, "/v lyckas")) {
                            PRINTFC_SERIAL("Choice: LYCKAS");
                            esp_mqtt_client_publish(mqtt_client, topic_player_uplink, "{\"val\": \"lyckas\"}", 0, 1, 0);
                        } else if (strstr((char *)data, "/v sabotage")) {
                            PRINTFC_SERIAL("Choice: SABOTAGE");
                            esp_mqtt_client_publish(mqtt_client, topic_player_uplink, "{\"val\": \"sabotage\"}", 0, 1, 0);
                        } else if (strstr((char *)data, "/v starta")) {
                            PRINTFC_SERIAL("Choice: STARTA SPEL");
                            esp_mqtt_client_publish(mqtt_client, topic_player_uplink, "{\"val\": \"starta\"}", 0, 1, 0);
                        } else if (data[0] == ':') {
                            PRINTFC_SERIAL("Chat message sent: %s", (data + 1));
                            char chat_message[512];
                            snprintf(chat_message, sizeof(chat_message), "{\"id\": \"%s\", \"data\": \"%s\"}", shorter_id, (data + 1));
                            esp_mqtt_client_publish(mqtt_client, TOPIC_TORGET, (char *)(chat_message), 0, 1, 0);
                        } else if (strstr((char *)data, "/r reg")) {
                            PRINTFC_SERIAL("Choice: REG_PLAYER");
                            xEventGroupSetBits(wifi_event_group, BIT3);
                        } else if (strstr((char *)data, "/r csr")) {
                            PRINTFC_SERIAL("Choice: SEND_CSR");
                            xEventGroupSetBits(wifi_event_group, BIT4);
                        } else if (strstr((char *)data , "/r start")) {
                            PRINTFC_SERIAL("Choice: START_GAME");
                            xEventGroupSetBits(wifi_event_group, BIT5);
                        } else {
                            PRINTFC_SERIAL("Unknown choice command.");
                        }
                    }
                    break;
                case UART_FIFO_OVF:
                    PRINTFC_SERIAL("UART FIFO overflow");
                    uart_flush_input(UART_NUM);
                    xQueueReset(serial_msg_queue);
                    break;

                case UART_BUFFER_FULL:
                    PRINTFC_SERIAL("UART buffer full");
                    uart_flush_input(UART_NUM);
                    xQueueReset(serial_msg_queue);
                    break;

                case UART_PARITY_ERR:
                    PRINTFC_SERIAL("UART parity error");
                    break;

                case UART_FRAME_ERR:
                    PRINTFC_SERIAL("UART frame error");
                    break;

                default:
                    PRINTFC_SERIAL("UART event type: %d", event.type);
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Adjust the delay as needed
    }
    vTaskDelete(NULL);
}