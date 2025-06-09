# For more information on build system, visit
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    components/esp-sr
    components/esp-dl
    components/esp-sr/esp-face/models/human_face_detect
    components/esp-sr/esp-face/models/human_face_recognition
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smartHome) 