#include "face_recognition.hpp"
#include "voice_recognition.hpp"

extern "C" void app_main(void)
{
    // Start face recognition service
    app_facerec_start();

    // Start voice recognition service
    app_voice_start();
} 