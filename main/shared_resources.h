#ifndef SHARED_RESOURCES_H
#define SHARED_RESOURCES_H

#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "mbedtls/pk.h"
#include "mqtt_client.h"

// Define "DEBUG_MODE" for more prints.
//#define DEBUG_MODE

#define TOPIC_TORGET "/torget"
#define TOPIC_MYNDIGHETEN "/myndigheten"

#define SERVER_IP "172.16.218.17"

//  wifi_event_group bits:
//  BIT0:   AP_CONNECTED
//  BIT1:   GOT_IP
//  BIT2:   CERT_SIGNED
//  BIT3:   REGISTER_PLAYER
//  BIT4:   SEND_CSR
//  BIT5:   START_GAME

extern SemaphoreHandle_t xSemaphore_serial;
extern SemaphoreHandle_t xSemaphore_mqtt_evt;
extern SemaphoreHandle_t xSemaphore_mqtt_client;
extern SemaphoreHandle_t xSemaphore_wifi_event;

extern QueueHandle_t mqtt_event_queue;
extern QueueHandle_t serial_msg_queue;
extern EventGroupHandle_t wifi_event_group;

extern char player_id[256];
extern char shorter_id[32];
extern char topic_player_uplink[64];

extern char signed_certificate[2048]; 
extern uint8_t key_pem[2048];

extern esp_mqtt_client_handle_t mqtt_client;

extern const uint8_t ca_server_copy[] asm("_binary_ca_crt_start");

#endif // SHARED_RESOURCES_H