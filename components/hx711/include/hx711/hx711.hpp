#pragma once

#include <driver/gpio.h>
#include <rom/ets_sys.h>
#include <rom/gpio.h>

class Hx711 {
public:
  enum class Gain : std::uint8_t { e128 = 1, e64 = 3, e32 = 2 };

  static constexpr bool LOW = false;
  static constexpr bool HIGH = (!LOW);
  static constexpr std::uint32_t CLOCK_DELAY_US = 20UL;
  static constexpr const char *TAG = "HX711";
  static constexpr float RAW_TO_GRAMMS = 107.73;

  explicit Hx711(gpio_num_t dout, gpio_num_t pd_sck, Gain gain)
      : m_dout{dout}, m_pd_sck{pd_sck}, m_gain{gain} {
    gpio_config_t io_conf{};
    io_conf.intr_type = static_cast<gpio_int_type_t>(GPIO_PIN_INTR_DISABLE);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1UL << m_pd_sck);
    io_conf.pull_down_en = static_cast<gpio_pulldown_t>(0);
    io_conf.pull_up_en = static_cast<gpio_pullup_t>(0);
    gpio_config(&io_conf);

    io_conf.intr_type = static_cast<gpio_int_type_t>(GPIO_PIN_INTR_DISABLE);
    io_conf.pin_bit_mask = (1UL << m_dout);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = static_cast<gpio_pullup_t>(0);
    gpio_config(&io_conf);

    m_offset = ReadAverage(20);
  }

  Hx711() = delete;
  Hx711(const Hx711&) = delete;
  Hx711& operator=(const Hx711&) = delete;
  Hx711(Hx711&&) = delete;
  Hx711& operator=(Hx711&&) = delete;
  ~Hx711() = default;

  [[nodiscard]] bool IsReady() const { return gpio_get_level(m_dout); }

  [[nodiscard]] std::uint32_t Read() const {
    gpio_set_level(m_pd_sck, LOW);
    // wait for the chip to become ready
    while (IsReady()) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    std::uint32_t ret{};
    portDISABLE_INTERRUPTS();

    for (int i = 0; i < 24; i++) {
      gpio_set_level(m_pd_sck, HIGH);
      ets_delay_us(CLOCK_DELAY_US);
      ret = ret << 1UL;
      gpio_set_level(m_pd_sck, LOW);
      ets_delay_us(CLOCK_DELAY_US);

      if (gpio_get_level(m_dout))
        ret++;
    }

    // set the channel and the gain factor for the next reading using the clock
    // pin
    for (unsigned i = 0; i < static_cast<unsigned>(m_gain); i++) {
      gpio_set_level(m_pd_sck, HIGH);
      ets_delay_us(CLOCK_DELAY_US);
      gpio_set_level(m_pd_sck, LOW);
      ets_delay_us(CLOCK_DELAY_US);
    }
    portENABLE_INTERRUPTS();

    ret = ret ^ 0x800000;

    return ret;
  }

  [[nodiscard]] std::uint32_t ReadAverage(std::uint8_t times) const {
    std::uint32_t sum{};
    for (std::uint8_t i{}; i < times; i++) {
      sum += Read();
    }
    return sum / static_cast<std::uint32_t>(times);
  }

  [[nodiscard]] std::uint32_t GetValue(std::uint8_t times) const {
    std::uint32_t avg = ReadAverage(times);
    return (avg > m_offset) ? (avg - m_offset) : 0UL;
  }

  [[nodiscard]] float GetUnits(std::uint8_t times) const {
    return static_cast<float>(GetValue(times)) / RAW_TO_GRAMMS;
  }

private:
  gpio_num_t m_dout;
  gpio_num_t m_pd_sck;
  Gain m_gain;
  std::uint32_t m_offset{}; // used for tare weight
};
