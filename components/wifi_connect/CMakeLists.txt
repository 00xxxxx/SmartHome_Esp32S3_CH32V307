# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
#
# The following five lines of boilerplate have to be in your project's
# top-level CMakeLists.txt file.
#
cmake_minimum_required(VERSION 3.16)

idf_component_register(SRCS "wifi_connect.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_wifi esp_event log nvs_flash lwip) 