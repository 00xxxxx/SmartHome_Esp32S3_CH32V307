#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/uart.h"

#include "mqtt_handler.h"

/* --- Alibaba Cloud IoT Credentials --- */
#define PRODUCT_KEY      "k1t73qLlqf2"
#define DEVICE_NAME      "esp32s3_01"

// IMPORTANT: Replace with the password you generated from the online tool
#define MQTT_PASSWORD    "PASTE_YOUR_GENERATED_PASSWORD_HERE"
/* ------------------------------------- */


// UART is shared with other components. We assume UART_NUM_1 has been initialized.
#define SHARED_UART_NUM     (UART_NUM_1)

static const char *TAG = "mqtt_handler";

// --- MQTT Connection Details (Auto-generated) ---
// Note: Do not change these unless you know what you are doing.
#define MQTT_BROKER_URI  "mqtts://" PRODUCT_KEY ".iot-as-mqtt.cn-shanghai.aliyuncs.com"
#define MQTT_PORT        1883
#define MQTT_CLIENT_ID   DEVICE_NAME "|securemode=3,signmethod=hmacsha1|"
#define MQTT_USERNAME    DEVICE_NAME "&" PRODUCT_KEY

// --- MQTT Topics ---
// Topic for subscribing to commands from the cloud
#define MQTT_TOPIC_SUB   "/" PRODUCT_KEY "/" DEVICE_NAME "/user/cmd"
// Topic for publishing status to the cloud (example)
#define MQTT_TOPIC_PUB   "/" PRODUCT_KEY "/" DEVICE_NAME "/user/status"


static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int)event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // Subscribe to the command topic
        msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC_SUB, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d, topic=%s", msg_id, MQTT_TOPIC_SUB);

        // Publish a "connected" message
        msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_PUB, "{\"status\":\"online\"}", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        // Check if the event is for our command topic
        if (strncmp(event->topic, MQTT_TOPIC_SUB, event->topic_len) == 0) {
            ESP_LOGI(TAG, "Received command: %.*s", event->data_len, event->data);

            // Forward command to CH32 via UART, similar to voice recognition
            // We append "\r\n" to match the expected format.
            char command_buffer[32];
            int len = snprintf(command_buffer, sizeof(command_buffer), "%.*s\r\n", event->data_len, event->data);
            uart_write_bytes(SHARED_UART_NUM, command_buffer, len);
            ESP_LOGI(TAG, "Forwarded command '%s' to CH32 via UART.", command_buffer);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void app_mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
    };

    ESP_LOGI(TAG, "Starting MQTT client...");
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
} 