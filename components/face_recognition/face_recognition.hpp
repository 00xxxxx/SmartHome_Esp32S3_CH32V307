#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts the face recognition task.
 *
 * This function creates a FreeRTOS task that continuously performs
 * face detection and recognition, and handles enrollment.
 * It also initializes all necessary hardware (Camera, UART) and systems (SPIFFS).
 */
void app_facerec_start(void);


#ifdef __cplusplus
}
#endif 