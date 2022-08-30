#include <chrono>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hx711/hx711.hpp>
#include <filter/filter.hpp>
#include <push-button/push-button.hpp>
#include <memory>
#include <message-queue/context.hpp>
#include <message-queue/interfaces.hpp>
#include <message-queue/scheduler.hpp>
#include <mqtt-helper/mqtt-helper.hpp>
#include <string>
#include <thread>
#include <nvs_flash.h>



#define AVG_SAMPLES   10
static const char* TAG = "HX711_TEST";



extern "C" void app_main(void)
{

    auto btn = std::make_unique<PushButton>(static_cast<gpio_num_t>(CONFIG_BUTTON_PIN));
    auto hx711 = std::make_unique<Hx711>(static_cast<gpio_num_t>(CONFIG_DATA_PIN), 
                                         static_cast<gpio_num_t>(CONFIG_SCLK_PIN), 
                                         Hx711::Gain::e128);
    float weight{hx711->GetUnits(AVG_SAMPLES)};
    auto kalman = std::make_unique<Filter>(0.5f, weight);

    while (true) 
    {
        // weight = kalman->filter(hx711->GetUnits(AVG_SAMPLES));
        // ESP_LOGI(TAG, "******* weight = %u", (unsigned)weight);
        std::this_thread::sleep_for(std::chrono::seconds{1});
    }
  }
} catch (const std::exception &e) {
  ESP_LOGE("Unhandled exception", "%s", e.what());
  std::this_thread::sleep_for(std::chrono::seconds{5U});
  esp_restart();
}
