#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connects the ESP32 to a Wi-Fi network.
 *
 * This function initializes the Wi-Fi station, connects to the configured
 * SSID, and waits until an IP address is obtained.
 */
void wifi_init_sta(void);

#ifdef __cplusplus
}
#endif 