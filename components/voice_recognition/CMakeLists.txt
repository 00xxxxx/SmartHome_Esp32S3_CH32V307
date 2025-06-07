idf_component_register(SRCS "voice_recognition.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp-sr esp_driver_i2s esp_driver_gpio esp_driver_uart) 