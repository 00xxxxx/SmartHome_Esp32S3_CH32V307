idf_component_register(SRCS "face_recognition.cpp"
                    INCLUDE_DIRS .
                    PRIV_INCLUDE_DIRS "../esp-sr/esp-face/models/human_face_detect" "../esp-sr/esp-face/models/human_face_recognition"
                    REQUIRES esp-sr esp32-camera human_face_detect human_face_recognition) 