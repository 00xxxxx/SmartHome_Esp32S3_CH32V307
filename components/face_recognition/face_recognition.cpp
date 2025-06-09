/**
 * @file face_recognition.cpp
 * @brief Implements face detection, enrollment, and recognition functionality.
 * @details This file sets up the camera, UART, and a GPIO button. It runs two FreeRTOS
 *          tasks: one to handle the enrollment button and another for the main
 *          face recognition loop. The recognized face ID or enrollment status is
 *          communicated over UART.
 */

// C/C++ and FreeRTOS
#include <vector>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-IDF Drivers and Systems
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_camera.h"
#include "driver/i2c.h"

// Face Recognition Headers
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"

// Public interface
#include "face_recognition.hpp"

// Tag for logging
static const char *TAG = "face_rec";

// Add model types from Kconfig
#include "sdkconfig.h"

// ==================================================================
//                          HARDWARE CONFIG
// ==================================================================
// Camera Pins
#define CAM_PIN_PWDN     -1
#define CAM_PIN_RESET    17
#define CAM_PIN_XCLK     -1
#define CAM_PIN_SIOD     14
#define CAM_PIN_SIOC     13
#define CAM_PIN_D7       9
#define CAM_PIN_D6       8
#define CAM_PIN_D5       7
#define CAM_PIN_D4       6
#define CAM_PIN_D3       5
#define CAM_PIN_D2       4
#define CAM_PIN_D1       2
#define CAM_PIN_D0       1
#define CAM_PIN_VSYNC    11
#define CAM_PIN_HREF     12
#define CAM_PIN_PCLK     10

// UART (for sending results)
#define UART_PORT_NUM      UART_NUM_1
#define UART_BAUD_RATE     115200
#define UART_TX_PIN        18
#define UART_RX_PIN        -1

// Enroll Button
#define ENROLL_BUTTON_GPIO GPIO_NUM_0 

// ==================================================================
//                      INTERNAL IMPLEMENTATION
// ==================================================================

/// Global objects for face detection, feature extraction, and recognition.
static HumanFaceDetect *g_detector = NULL;
static HumanFaceFeat *g_feat = NULL;
static HumanFaceRecognizer *g_recognizer = NULL;

/// Volatile flag to signal the recognition task to start enrollment.
static volatile uint8_t g_is_enrolling = 0;

// -- Initialization Functions --

/**
 * @brief Initializes the SPIFFS file system.
 * @details Mounts the SPIFFS partition labeled "storage". This is used to store
 *          the enrolled face database.
 */
static void spiffs_init()
{
    ESP_LOGI(TAG, "Initializing SPIFFS...");
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS initialized.");
    }
}

/**
 * @brief Initializes the UART interface.
 * @details Configures UART for sending recognition and enrollment results.
 *          This component shares the UART with the voice recognition component.
 */
static void uart_init()
{
    // Use C++ zero-initialization to ensure all fields are initialized.
    uart_config_t uart_config = {};
    uart_config.baud_rate = UART_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, 1024, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "UART initialized.");
}

/**
 * @brief Initializes the camera module.
 * @details Configures the camera with the specified GPIO pins, pixel format,
 *          frame size, and other parameters.
 */
static void camera_init(void) {
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN, .pin_reset = CAM_PIN_RESET, .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD, .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7, .pin_d6 = CAM_PIN_D6, .pin_d5 = CAM_PIN_D5, .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3, .pin_d2 = CAM_PIN_D2, .pin_d1 = CAM_PIN_D1, .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC, .pin_href = CAM_PIN_HREF, .pin_pclk = CAM_PIN_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0, .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_RGB888,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 12, .fb_count = 2, .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .sccb_i2c_port = I2C_NUM_0,
    };
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
    } else {
        ESP_LOGI(TAG, "Camera initialized.");
    }
}

// -- RTOS Tasks --

/**
 * @brief FreeRTOS task to monitor the enrollment button.
 * @details This task polls a GPIO pin connected to a button. When the button is
 *          pressed, it sets a global flag to trigger the enrollment process
 *          in the main recognition task.
 * @param arg Task arguments (unused).
 */
static void enroll_button_task(void *arg) {
    gpio_set_direction(ENROLL_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ENROLL_BUTTON_GPIO, GPIO_PULLUP_ONLY);
    while (true) {
        if (gpio_get_level(ENROLL_BUTTON_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
            if (gpio_get_level(ENROLL_BUTTON_GPIO) == 0) {
                ESP_LOGI(TAG, "Enroll button pressed. Starting enrollment...");
                g_is_enrolling = 1;
                while(gpio_get_level(ENROLL_BUTTON_GPIO) == 0) { // Wait for release
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Main FreeRTOS task for face recognition and enrollment.
 * @details This task initializes the face detection and recognition models. It then
 *          enters a loop to continuously capture frames from the camera. If the
 *          enrollment flag is set, it attempts to enroll a new face. Otherwise,
 *          it performs face recognition. Results are sent over UART.
 * @param arg Task arguments (unused).
 */
static void face_recognition_task(void *arg) {
    // 1. Initialize face detection and recognition models.
    // The specific models used are determined by Kconfig settings.
    g_detector = new HumanFaceDetect();
    g_feat = new HumanFaceFeat();
    // The face database is stored in SPIFFS.
    char *db_path = (char *)"/spiffs/face_db";
    g_recognizer = new HumanFaceRecognizer(g_feat, db_path, 0.5F, 5);
    
    ESP_LOGI(TAG, "Face recognition task started. Press button on GPIO %d to enroll.", ENROLL_BUTTON_GPIO);

    // 2. Main recognition and enrollment loop
    while (true) {
        // Capture a frame from the camera
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Failed to get camera frame buffer");
            continue;
        }

        // Create a dl_matrix3du_t image object from the frame buffer
        dl::image::img_t img;
        img.width = fb->width;
        img.height = fb->height;
        img.data = fb->buf;
        img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;

        // 3. Check if enrollment is requested
        if (g_is_enrolling) {
            // Detect face in the frame
            auto result = g_detector->run(img);
            if(!result.empty()) {
                // Enroll the detected face
                int8_t enroll_id = g_recognizer->enroll(img, result);
                 if (enroll_id >= 0) {
                    ESP_LOGI(TAG, "Enrollment successful for ID: %d", enroll_id);
                    char msg[32];
                    sprintf(msg, "ENROLLED:%d\r\n", enroll_id);
                    uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
                } else {
                    ESP_LOGW(TAG, "Enrollment failed.");
                }
            } else {
                 ESP_LOGW(TAG, "Enrollment failed: No face detected.");
            }
            // Reset the enrollment flag
            g_is_enrolling = 0;
        } else {
            // 4. Perform face recognition
            // Detect face in the frame
            auto result_detect = g_detector->run(img);
            if(!result_detect.empty()){
                // Recognize the detected face
                auto results_recog = g_recognizer->recognize(img, result_detect);
                if (results_recog[0].id > -1) {
                    ESP_LOGI(TAG, "Recognition successful. ID: %d", results_recog[0].id);
                    const char *msg = "ReeSuccess\r\n";
                    uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
                } else {
                    ESP_LOGI(TAG, "Recognition failed: Unknown face detected.");
                    const char *msg = "ReFail\r\n";
                    uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
                }
            } else {
                ESP_LOGI(TAG, "Recognition failed: No face detected.");
                const char *msg = "ReFail\r\n";
                uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
            }
        }
        // Return the frame buffer to be reused
        esp_camera_fb_return(fb); 
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to yield CPU
    }
}

// ==================================================================
//                           PUBLIC INTERFACE
// ==================================================================
/**
 * @brief Starts the face recognition functionality.
 * @details Initializes all necessary subsystems (SPIFFS, UART, Camera) and
 *          creates the FreeRTOS tasks for button handling and face recognition.
 */
void app_facerec_start(void)
{
    // Initialize all systems first
    spiffs_init();
    uart_init();
    camera_init();

    // Create RTOS tasks
    xTaskCreate(enroll_button_task, "enroll_btn", 2048, NULL, 5, NULL);
    xTaskCreate(face_recognition_task, "face_rec", 8192, NULL, 5, NULL);
} 