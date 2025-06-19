/**
 * @file voice_recognition.cpp
 * @brief Implements voice recognition functionality using ESP-SR.
 * @details This file contains the setup for I2S microphone input and a FreeRTOS task
 *          that uses the ESP-SR MultiNet model to recognize predefined speech commands.
 */

// C/C++ and FreeRTOS
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-IDF Drivers and Systems
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/uart.h" // For uart_write_bytes
#include "esp_log.h"
#include "esp_check.h"

// ESP-SR (Speech Recognition)
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "model_path.h"

// Public interface
#include "voice_recognition.hpp"

// Tag for logging
static const char *TAG = "voice_rec";

// ==================================================================
//                          HARDWARE CONFIG
// ==================================================================
// I2S for INMP441 Microphone
#define I2S_BCK_IO          (GPIO_NUM_36)
#define I2S_WS_IO           (GPIO_NUM_37)
#define I2S_DATA_IN_IO      (GPIO_NUM_35)
#define I2S_NUM             (I2S_NUM_0)
#define I2S_SAMPLE_RATE     (16000)
#define I2S_READ_LEN        (1600 * 2) // 200ms audio buffer

// UART is shared with the face_recognition component.
// We assume UART_NUM_1 has already been initialized.
#define SHARED_UART_NUM     (UART_NUM_1)


// ==================================================================
//                      INTERNAL IMPLEMENTATION
// ==================================================================

/// I2S channel handle for the microphone
static i2s_chan_handle_t rx_handle;

/**
 * @brief Initializes the I2S interface for the INMP441 microphone.
 * @details Configures I2S in standard mode with the specified sample rate and GPIO pins
 *          to capture audio data from the microphone.
 */
static void i2s_init() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA_IN_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    ESP_LOGI(TAG, "I2S for INMP441 initialized.");
}

/**
 * @brief FreeRTOS task for continuous speech recognition.
 * @details This task initializes the MultiNet speech recognition model, sets up a list of
 *          custom commands, and enters a loop to continuously process audio from the
 *          I2S microphone. When a command is detected, it sends a corresponding
 *          string over UART.
 * @param arg Task arguments (unused).
 */
static void speech_recognition_task(void *arg) {
    // 1. Initialize speech recognition model
    srmodel_list_t *models = esp_srmodel_init("model");
    char *model_name = esp_srmodel_filter(models, ESP_MN_PREFIX, NULL);
    esp_mn_iface_t *multinet = (esp_mn_iface_t *)esp_mn_handle_from_name(model_name);
    model_iface_data_t *model_data = multinet->create(model_name, 6000);

    // 2. Define custom speech commands in Pinyin
    char cmd_turn_on[] = "da kai deng";
    char cmd_turn_off[] = "guan deng";
    char cmd_collect[] = "cai ji yi ci shu ju";

    // 3. Create phrase and node structures for the command list
    // Each phrase is linked together to form a command list for the model.
    esp_mn_phrase_t phrase_turn_on = {cmd_turn_on, NULL, 1, 0, NULL};
    esp_mn_phrase_t phrase_turn_off = {cmd_turn_off, NULL, 2, 0, NULL};
    esp_mn_phrase_t phrase_collect = {cmd_collect, NULL, 3, 0, NULL};

    esp_mn_node_t node_collect = {.phrase = &phrase_collect, .next = NULL};
    esp_mn_node_t node_turn_off = {.phrase = &phrase_turn_off, .next = &node_collect};
    esp_mn_node_t node_turn_on = {.phrase = &phrase_turn_on, .next = &node_turn_off};

    // 4. Set the custom commands in the MultiNet model
    multinet->set_speech_commands(model_data, &node_turn_on);
    
    // 5. Get audio configuration and allocate buffer
    int audio_chunksize = multinet->get_samp_chunksize(model_data);
    int16_t *buffer = (int16_t *)malloc(audio_chunksize * sizeof(int16_t));
    assert(buffer);

    ESP_LOGI(TAG, "Speech recognition task started. Say a command.");

    // 6. Main recognition loop
    while(1) {
        size_t bytes_read = 0;
        // Read audio data from the I2S microphone
        i2s_channel_read(rx_handle, buffer, audio_chunksize * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        if (bytes_read > 0) {
            // Feed audio data to the speech recognition model
            esp_mn_state_t mn_state = multinet->detect(model_data, buffer);

            if (mn_state == ESP_MN_STATE_DETECTED) {
                // If a command is detected, get the result
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                ESP_LOGI(TAG, "Command detected, id: %d", mn_result->command_id[0]);
                
                // Handle the detected command based on its ID
                if (mn_result->command_id[0] == 1) { // da kai deng
                    uart_write_bytes(SHARED_UART_NUM, "LED2ON\r\n", 8);
                } else if (mn_result->command_id[0] == 2) { // guan deng
                    uart_write_bytes(SHARED_UART_NUM, "LED2OFF\r\n", 9);
                } else if (mn_result->command_id[0] == 3) { // cai ji yi ci shu ju
                    uart_write_bytes(SHARED_UART_NUM, "Collect\r\n", 9);
                }
            }
        }
    }
    
    // 7. Cleanup resources
    // This part is unreachable in the current implementation but is good practice.
    free(buffer);
    multinet->destroy(model_data);
    esp_srmodel_deinit(models);
    vTaskDelete(NULL);
}


// ==================================================================
//                           PUBLIC INTERFACE
// ==================================================================
/**
 * @brief Starts the voice recognition functionality.
 * @details Initializes the necessary hardware (I2S) and creates the FreeRTOS task
 *          for speech recognition. Assumes UART has been initialized elsewhere.
 */
void app_voice_start(void)
{
    // Initialize hardware
    i2s_init();

    // NOTE: UART is not initialized here.
    // It is assumed to be initialized by the face_recognition component.

    ESP_LOGI(TAG, "Voice recognition module starting.");
    const char *msg = "VOICE_READY\r\n";
    uart_write_bytes(SHARED_UART_NUM, msg, strlen(msg));

    // Create the speech recognition task
    xTaskCreate(speech_recognition_task, "speech_recognition", 8192, NULL, 5, NULL);
} 