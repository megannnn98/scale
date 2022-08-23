#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <hx711/hx711.hpp>
#include <memory>

#define AVG_SAMPLES   10
#define GPIO_DATA   GPIO_NUM_2
#define GPIO_SCLK   GPIO_NUM_16
static const char* TAG = "HX711_TEST";

extern "C" void app_main(void)
{

    auto hx711 = std::make_unique<Hx711>(GPIO_DATA, GPIO_SCLK, Hx711::Gain::e128);

    float weight{};
    while (true) 
    {
        weight = hx711->GetUnits(AVG_SAMPLES);
        ESP_LOGI(TAG, "******* weight = %u", (unsigned)weight);
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }
}
