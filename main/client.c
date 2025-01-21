#ifndef CLIENT_H
#define CLIENT_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"

#include "shared_resources.h"

#include "printer_helper.h"
#include "esp_http_client.h"
#include "esp_http_client.h"
#include "mqtt_handler.c"
#include "serial.c"
#include "freertos/queue.h"
#include "generate_csr.c"
#include "esp_event.h"
#include "esp_tls.h"



void client_start();

void register_player();

void send_csr(const char *csr);

void start_game();


#endif



//#include "printer_helper.h" 
//#include "mqtt_handler.h"
//#include "serial.h"
//#include "generate_csr.h"


#define SERVER_REGISTER_URL "https://" SERVER_IP ":9191/spelare"
#define SERVER_START_URL    "https://" SERVER_IP ":9191/start"
#define CSR_ENDPOINT        "https://" SERVER_IP ":9191/spelare/csr"

#define MAX_HTTP_RECV_BUFFER 2048
#define MAX_HTTP_OUTPUT_BUFFER 2048

esp_mqtt_client_handle_t mqtt_client;

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char *output_buffer;
    static int output_len;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            PRINTFC_CLIENT("HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            PRINTFC_CLIENT("HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            PRINTFC_CLIENT("HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            PRINTFC_CLIENT("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            PRINTFC_CLIENT("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            PRINTFC_CLIENT("Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            PRINTFC_CLIENT("HTTP_EVENT_ON_FINISH");
            char *cert_start = strstr((char*)output_buffer, "-----BEGIN CERTIFICATE-----");
                if (cert_start) {
                    char *cert_end = strstr(cert_start, "-----END CERTIFICATE-----");
                    if (cert_end) {
                        cert_end += strlen("-----END CERTIFICATE-----"); // Move to the end of the certificate

                        int cert_len = cert_end - cert_start;
                        if (cert_len < sizeof(signed_certificate)) {
                            memset(signed_certificate, 0, sizeof(signed_certificate));
                            strncpy(signed_certificate, cert_start, cert_len);
                            signed_certificate[cert_len] = '\0'; // Ensure null-termination
#ifdef DEBUG_MODE
                            PRINTFC_CLIENT("Signed certificate: %s", signed_certificate);
#endif
                            // Signal that the signed certificate has been received
                            xEventGroupSetBits(wifi_event_group, BIT2);
                        } else {
                            PRINTFC_CLIENT("Error: Signed certificate buffer too small");
                        }
                    } else {
                        PRINTFC_CLIENT("Error: Missing 'END CERTIFICATE' marker");
                    }
                } else {
                    // {"id": "%s"}
                    char *player_id_start = strstr((char*)output_buffer, "{\"id");
                    if (player_id_start) {
                        player_id_start += strlen("{\"id\":\"");
                        char *player_id_end = strstr(player_id_start, "\"}");
                        if (player_id_end) {
                            int player_id_len = player_id_end - player_id_start;
                            if (player_id_len < sizeof(player_id)) {
                                memset(player_id, 0, sizeof(player_id));
                                strncpy(player_id, player_id_start, player_id_len);
                                player_id[player_id_len] = '\0'; // Ensure null-termination
                                PRINTFC_CLIENT("Player ID: %s", player_id);
                            } else {
                                PRINTFC_CLIENT("Error: Player ID buffer too small");
                            }
                        } else {
                            PRINTFC_CLIENT("Error: Missing end quote for player ID");
                        }
                    } else {
                        PRINTFC_CLIENT("Not a player ID response");
                    }
                }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            PRINTFC_CLIENT("HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                PRINTFC_CLIENT("Last esp error code: 0x%x", err);
                PRINTFC_CLIENT("Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            PRINTFC_CLIENT("HTTP_EVENT_REDIRECT");
            break;
        default:
            PRINTFC_CLIENT("Other event id: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

void client_task()
{
    PRINTFC_CLIENT("Client started and waiting for Wi-Fi to connect");

    //wait wifi to connect
    xEventGroupWaitBits(wifi_event_group, BIT0 | BIT1, pdFALSE, pdTRUE, portMAX_DELAY);

    PRINTFC_CLIENT("Starting serial task");
    xTaskCreate(serial_task, "serial task", 16384, NULL, 5, NULL);

    xEventGroupWaitBits(wifi_event_group, BIT3, pdFALSE, pdTRUE, portMAX_DELAY);
    PRINTFC_CLIENT("Sending player registration request");
    register_player();

    // Generate and send CSR
    xEventGroupWaitBits(wifi_event_group, BIT4, pdFALSE, pdTRUE, portMAX_DELAY);
    char csr[2048];
    generate_csr(csr, sizeof(csr), player_id); // Use the actual player ID
#ifdef DEBUG_MODE
    PRINTFC_CLIENT("player_id before generating CSR: %s", player_id);
    PRINTFC_CLIENT("CSR: %s", csr);
#endif    
    PRINTFC_CLIENT("Sending CSR");
    send_csr(csr);

    // wait for cert
    xEventGroupWaitBits(wifi_event_group, BIT0 | BIT1 | BIT2, pdFALSE, pdTRUE, portMAX_DELAY);

    xEventGroupWaitBits(wifi_event_group, BIT5, pdFALSE, pdTRUE, portMAX_DELAY);
    PRINTFC_CLIENT("Sending game start request");
    start_game();

    // Initialize the MQTT client and return the handle
    mqtt_client = mqtt_app_start();
   
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

void client_start()
{
    PRINTFC_CLIENT("Client starting");

    if (xTaskCreate(client_task, "client task", 16384, NULL, 5, NULL) != pdPASS) {
        PRINTFC_CLIENT("Failed to create client task");
    }
}

void register_player()
{
#ifdef DEBUG_MODE
    PRINTFC_CLIENT("ca_server_copy: %s", (const char*)ca_server_copy);
    PRINTFC_CLIENT("Register URL: %s", SERVER_REGISTER_URL);
#endif    
    esp_http_client_config_t config = {
        .url = SERVER_REGISTER_URL,
        .cert_pem = (const char*)ca_server_copy, // Server's certificate for verification
        .skip_cert_common_name_check = true,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL) {
        PRINTFC_CLIENT("Failed to initialize HTTP client");
        return;
    }
    char* json = "{}";
    size_t json_len =sizeof(json);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, json_len); 

    esp_err_t err = esp_http_client_perform(client);

    PRINTFC_CLIENT("Performing HTTP POST errno: %s", esp_err_to_name(err));

    esp_http_client_cleanup(client);
}


void send_csr(const char *csr)
{
    PRINTFC_CLIENT("CSR URL: %s", CSR_ENDPOINT);
    esp_http_client_config_t config = {
        .url = CSR_ENDPOINT,
        .cert_pem = (const char *)ca_server_copy,
        .skip_cert_common_name_check = true, // test, remove later
        .common_name = player_id,
        .event_handler = _http_event_handler,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        PRINTFC_CLIENT("Failed to initialize HTTP client");
        return;
    }

    // Configure HTTP method and payload
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, csr, strlen(csr));

    // HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        PRINTFC_CLIENT("CSR submitted successfully.");
        PRINTFC_CLIENT("HTTP Status = %d, Content-Length = %" PRId64,
                       esp_http_client_get_status_code(client),
                       esp_http_client_get_content_length(client));
    } else {
    PRINTFC_CLIENT("Error sending CSR: %s", esp_err_to_name(err));
    }

    // clean client
    esp_http_client_cleanup(client);
}


void start_game()
{
    PRINTFC_CLIENT("Game start URL: %s", SERVER_START_URL);
    const char *json_payload = "{\"val\": \"nu kör vi\"}";

    esp_http_client_config_t config = {
        .url = SERVER_START_URL,
        .cert_pem = (const char *)ca_server_copy,
        .client_cert_pem = (const char *)signed_certificate,
        .client_key_pem = (const char *)key_pem,
        .skip_cert_common_name_check = true,
        .event_handler = _http_event_handler,
        .common_name = player_id,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL) {
        PRINTFC_CLIENT("Failed to initialize HTTP client");
        return;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        PRINTFC_CLIENT("Game start request sent successfully.");
    } else {
        PRINTFC_CLIENT("Error starting game: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

