idf_component_register(SRCS "main.cpp" "person_detect_model_data.cc"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash esp-tflite-micro esp32-camera esp_timer)