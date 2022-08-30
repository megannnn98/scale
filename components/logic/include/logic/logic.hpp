#pragma once
#include "esp_log.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <message-queue/interfaces.hpp>
#include <utils/utils.hpp>
#include <mqtt-helper/mqtt-helper.hpp>
#include <weight-meter/weight-meter.hpp>

class Logic final : public mq::ISystem {
  static constexpr const char *TAG = "Logic";
  static constexpr auto CHECK_TIME{std::chrono::seconds{5}};

  mq::IContext &m_ctx;
  mq::IScheduler &m_scheduler;
  std::shared_ptr<mqtt::Client> m_mqttClient;
public:
  enum class Event : decltype(mq::Addr::ev) {
    eStartReadWeight,
    eGotWeight,
  };

  Logic(mq::IContext& ctx, 
        mq::IScheduler& scheduler,
        std::shared_ptr<mqtt::Client> mqttClient)
      : m_ctx{ctx}, 
        m_scheduler{scheduler}, 
        m_mqttClient{std::move(mqttClient)} 
  {
    m_ctx.Push(mq::Message{
        mq::NONE,
        mq::Addr{GetId(), utils::EnumValue(Event::eStartReadWeight)},
        {}});
  }

  void Process(const mq::Message &msg) {
    if (msg.to.sys != GetId())
      return;
    switch (msg.to.ev) {
    case utils::EnumValue(Event::eStartReadWeight): {
      m_ctx.Push(mq::Message{
          mq::Addr{GetId(), utils::EnumValue(Event::eGotWeight)},
          mq::Addr{mq::Id::eWeightMeter, utils::EnumValue(WeightMeter::Event::eReadCmd)},
          {}});
      m_scheduler.ScheduleAfter(
        mq::Message{ 
        mq::NONE,
        mq::Addr{GetId(), utils::EnumValue(Event::eStartReadWeight)},
        {}}, 
        m_ctx.GetNumPriorities() - 1,
        std::chrono::seconds{CHECK_TIME});
    } break;
    case utils::EnumValue(Event::eGotWeight): {
      ESP_LOGI(TAG, "GotWeight");
    } break;
    }
  }

  [[nodiscard]] mq::Id GetId() const noexcept final {
    return mq::Id::eLogic;
  }
};
