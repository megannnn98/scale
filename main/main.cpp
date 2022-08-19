#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "HX711.h"



extern "C" void app_main(void)
{

    
    if (HX711_is_ready())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    while (true) {

        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}
