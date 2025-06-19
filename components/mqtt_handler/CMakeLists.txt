idf_component_register(SRCS "mqtt_handler.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_event mqtt log esp_driver_uart) 