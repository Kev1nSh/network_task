#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H



esp_mqtt_client_handle_t mqtt_app_start();

void mqtt_message_handler(void *event_data);

#endif

#include "mqtt_client.h"
#include "printer_helper.h"
#include "shared_resources.h"
#include "esp_log.h"
#include "esp_event.h"

#define MAX_PLAYER_ID_LEN 32
#define MAX_TOPIC_LEN (17 + MAX_PLAYER_ID_LEN + 1) // 17 for string + id + \0 

#define MQTT_BROKER_URI "mqtts://" SERVER_IP ":8884" // 8883 or 8884

#define MAX_MSG_LEN 128

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        PRINTFC_MQTT("Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
#ifdef DEBUG_MODE
    PRINTFC_MQTT("Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
#endif
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        PRINTFC_MQTT("MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/torget", 0);
        if (msg_id == -1) {
            PRINTFC_SERIAL("Failed to subscribe to /torget.");
        } else 
            PRINTFC_MQTT("Subscribed to /torget, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, "/myndigheten", 0);
        if (msg_id == -1) {
            PRINTFC_SERIAL("Failed to subscribe to /myndigheten.");
        } else
            PRINTFC_MQTT("Subscribed to /myndigheten, msg_id=%d", msg_id);
        
        // Subscribe to player-specific topics
        char downlink_topic[MAX_TOPIC_LEN];
        snprintf(downlink_topic, sizeof(downlink_topic), "/spelare/%s/downlink", shorter_id);
        msg_id = esp_mqtt_client_subscribe(client, downlink_topic, 0);
        if (msg_id == -1) {
            PRINTFC_SERIAL("Failed to subscribe to /myndigheten.");
        } else 
            PRINTFC_MQTT("Subscribed to %s, msg_id=%d", downlink_topic, msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        PRINTFC_MQTT("MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        PRINTFC_MQTT("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        PRINTFC_MQTT("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        PRINTFC_MQTT("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        PRINTFC_MQTT("MQTT_EVENT_DATA, topic: %.*s\r", event->topic_len, event->topic);
        mqtt_message_handler(event_data);
        break;
    case MQTT_EVENT_ERROR:
        PRINTFC_MQTT("MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            PRINTFC_MQTT("Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        PRINTFC_MQTT("Other event id:%d", event->event_id);
        break;
    }
}

// Initialize MQTT Client
esp_mqtt_client_handle_t mqtt_app_start()
{
    xEventGroupWaitBits(wifi_event_group, BIT0 | BIT1 | BIT2, pdFALSE, pdTRUE, portMAX_DELAY);
    PRINTFC_MQTT("MQTT app starting");
#ifdef DEBUG_MODE
    esp_log_level_set("esp-tls", ESP_LOG_DEBUG);
    esp_log_level_set("mbedtls", ESP_LOG_DEBUG);
    PRINTFC_MQTT("key_pem after type conversion: %s", (const char *)key_pem);
#endif
    PRINTFC_MQTT("Broker address: %s", MQTT_BROKER_URI);
    strncpy(shorter_id, player_id, 32);
    snprintf(topic_player_uplink, sizeof(topic_player_uplink), "/spelare/%s/uplink", shorter_id);

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = MQTT_BROKER_URI,
            .verification = {
                .certificate = (const char*)ca_server_copy,
                .skip_cert_common_name_check = true,
            },
        },

        .credentials = {
            .authentication = {
                .certificate = (const char*)signed_certificate,
                .key = (const char *)key_pem,
            },
            .client_id = player_id,
        },
        .network.timeout_ms = 10000, // Increase timeout to 10 seconds
    };
        
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        PRINTFC_MQTT("Failed to initialize MQTT client");
        return NULL;
    }

    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        PRINTFC_MQTT("Failed to start MQTT client: %s", esp_err_to_name(err));
        return NULL;
    }

    return client;
}

void mqtt_message_handler(void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
#ifdef DEBUG_MODE
    PRINTFC_MQTT("Broker message received: \ntopic: %s \ndata: %s", event->topic, event->data);
#endif    
    if (strstr(event->topic, "/torget")) {
        char chat_msg[MAX_MSG_LEN];
        const char *payload = event->data; // Use the payload directly

        // Find the start and end of the JSON message within the payload
        const char *msg_start = strchr(payload, '{'); // Find the opening brace
        const char *msg_end = strchr(payload, '}');   // Find the closing brace

        if (msg_start && msg_end && msg_end > msg_start) {
            // Calculate the length of the message, ensuring it fits in chat_msg
            int msg_len = msg_end - msg_start + 1; // Include the closing brace
            if (msg_len < MAX_MSG_LEN) {
                strncpy(chat_msg, msg_start, msg_len);
                chat_msg[msg_len] = '\0'; // Null-terminate the string
                PRINTFC_CHAT(chat_msg);   // Print the extracted message
            } else {
                PRINTFC_CHAT("Message too long to process.");
            }
        } else {
            PRINTFC_CHAT("Malformed message received.");
        }
    } else if (strstr(event->topic, "/myndigheten")) {
        PRINTFC_MYNDIGHETEN(event->data);
    } else if (strstr(event->topic, "/spelare/")) {
        PRINTFC_DOWNLINK(event->data);
    } else {
        PRINTFC_MQTT("Unhandled topic: %s", event->topic);
    }
}