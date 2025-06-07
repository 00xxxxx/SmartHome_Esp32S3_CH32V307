set(srcs "main.cpp")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES face_recognition voice_recognition) 