#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts the MQTT client and connects to the Alibaba Cloud IoT platform.
 *
 * This function configures the MQTT client with the pre-defined credentials
 * and starts the connection process. It also registers the event handler
 * to process incoming messages and other MQTT events.
 */
void app_mqtt_start(void);

#ifdef __cplusplus
}
#endif 