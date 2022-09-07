#pragma once

#include "iot_button.h"

class PushButton {

  gpio_num_t pin_;
  button_handle_t handle_;
public:
    
  static void button_press_down_cb(void*, void*)
  {
      // TEST_ASSERT_EQUAL_HEX(BUTTON_PRESS_DOWN, iot_button_get_event(arg));
      // ESP_LOGI(TAG, "BTN%d: BUTTON_PRESS_DOWN", get_btn_index((button_handle_t)arg));
  }

  PushButton(gpio_num_t pin)
  : pin_{pin}
  {
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = pin_,
            .active_level = 0,
        },
    };
    handle_ = iot_button_create(&cfg);
    iot_button_register_cb(handle_, BUTTON_PRESS_DOWN, button_press_down_cb, nullptr);
  }

  PushButton(const PushButton&) = delete;
  PushButton& operator=(const PushButton&) = delete;
  PushButton(PushButton&&) = delete;
  PushButton& operator=(PushButton&&) = delete;
  ~PushButton()
  {
    iot_button_unregister_cb(handle_, BUTTON_PRESS_DOWN);
    iot_button_delete(handle_);
  }

};
