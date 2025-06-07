idf_component_register(SRCS "${CMAKE_CURRENT_LIST_DIR}/face_recognition.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_cam esp-dl esp32-camera spiffs esp-face) 