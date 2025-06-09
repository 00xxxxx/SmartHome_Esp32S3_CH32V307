#pragma once

/**
 * @brief Starts the voice recognition task.
 * 
 * This function initializes the necessary hardware (I2S for INMP441) and
 * creates a FreeRTOS task to handle continuous voice recognition.
 */
void app_voice_start(void); 