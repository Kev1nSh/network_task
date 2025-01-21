#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_HAS_IP_BIT BIT1
#define WIFI_RECONNECT_MAX_ATTEMPT 100

typedef struct wifi_init_param_t
{
    EventGroupHandle_t wifi_event_group;
    char ssid[32];
    char password[64];
} wifi_init_param_t;

void wifi_handler_start(wifi_init_param_t *param);

#endif


#include "printer_helper.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static int reconnect_counter = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_init_param_t *param = (wifi_init_param_t *)arg;
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:

        esp_wifi_connect();
        break;

    case WIFI_EVENT_STA_CONNECTED:
        xEventGroupSetBits(param->wifi_event_group, WIFI_CONNECTED_BIT);
        PRINTFC_WIFI_HANDLER("Connected to AP!!!");
        reconnect_counter = 0;
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(param->wifi_event_group, WIFI_CONNECTED_BIT | WIFI_HAS_IP_BIT);
        if (reconnect_counter < WIFI_RECONNECT_MAX_ATTEMPT)
        {
            reconnect_counter++;
            esp_wifi_connect();
        }
        break;

    default:
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_init_param_t *param = (wifi_init_param_t *)arg;
    switch (event_id)
    {
    case IP_EVENT_STA_GOT_IP:
        PRINTFC_WIFI_HANDLER("Got IP");
        xEventGroupSetBits(param->wifi_event_group, WIFI_HAS_IP_BIT);
        break;

    default:
        break;
    }
}

void wifi_handler_start(wifi_init_param_t *param)
{
    PRINTFC_WIFI_HANDLER("WiFi Handler is starting");

    PRINTFC_WIFI_HANDLER("Using ssid: %s%s%s", green, param->ssid, reset);
    PRINTFC_WIFI_HANDLER("Using password: %s%s%s", green, param->password, reset);

    PRINTFC_WIFI_HANDLER("Init network interface");
    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    assert(netif != NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, param, NULL);
    PRINTFC_WIFI_HANDLER("Wifi event handler registered");
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, param, NULL));
    PRINTFC_WIFI_HANDLER("IP event handler registered");

    wifi_config_t wifi_config = {
        .sta = {
            .pmf_cfg = {
                .required = false,  
            },
        },
    };
    
    memcpy(wifi_config.sta.ssid, param->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, param->password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    PRINTFC_WIFI_HANDLER("Wifi is starting");
}