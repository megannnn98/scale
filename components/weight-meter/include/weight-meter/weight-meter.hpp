#pragma once
#include "esp_log.h"
#include <chrono>
#include <cstdint>
#include <hx711/hx711.hpp>
#include <memory>
#include <message-queue/interfaces.hpp>
#include <utils/utils.hpp>

class WeightMeter final : public mq::ISystem {
  static constexpr const char *TAG = "WEIGHT-METER";
  static constexpr const int AVG_SAMPLES = 10;
  static constexpr auto CHECK_TIME{std::chrono::seconds{5}};

  mq::IContext &m_ctx;
  mq::IScheduler &m_scheduler;
  std::unique_ptr<Hx711> m_hx711;

public:
  enum class Event : decltype(mq::Addr::ev) {
    eReadCmd,
  };

  WeightMeter(mq::IContext &ctx, mq::IScheduler &scheduler,
              std::unique_ptr<Hx711> hx711)
      : m_ctx{ctx}, m_scheduler{scheduler}, m_hx711{std::move(hx711)} {
    m_ctx.Push(mq::Message{
        mq::NONE,
        mq::Addr{GetId(), utils::EnumValue(Event::eReadCmd)},
        {}});
  }

  void Process(const mq::Message &msg) {
    if (msg.to.sys != GetId())
      return;

    switch (msg.to.ev) {
    case utils::EnumValue(Event::eReadCmd): {
      float weight = m_hx711->GetUnits(AVG_SAMPLES);
      ESP_LOGI(TAG, "%u", (unsigned)weight);
      m_ctx.Push(mq::Message{ msg.to, msg.from, weight});
    } break;
    }
  }

  [[nodiscard]] mq::Id GetId() const noexcept final {
    return mq::Id::eWeightMeter;
  }
};
