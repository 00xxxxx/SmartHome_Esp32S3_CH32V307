#include "face_recognition.hpp"
#include "voice_recognition.hpp"

// Include the new handlers
#include "wifi_connect.h"
#include "mqtt_handler.h"


extern "C" void app_main(void)
{
    // 1. Connect to Wi-Fi
    wifi_init_sta();

    // 2. Start the MQTT client
    app_mqtt_start();

    // 3. Start existing services
    // Start face recognition service
    app_facerec_start();

    // Start voice recognition service
    app_voice_start();
} 