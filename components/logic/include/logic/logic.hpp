#pragma once
#include "esp_log.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <message-queue/interfaces.hpp>
#include <utils/utils.hpp>

class Logic final : public mq::ISystem {
  static constexpr const char *TAG = "Logic";

  mq::IContext &m_ctx;
  mq::IScheduler &m_scheduler;
  std::shared_ptr<mqtt::Client> m_mqttClient;
public:
  enum class Event : decltype(mq::Addr::ev) {
    eReadCmd,
  };

  Logic(mq::IContext& ctx, 
        mq::IScheduler& scheduler,
        std::shared_ptr<mqtt::Client> mqttClient)
      : m_ctx{ctx}, 
        m_scheduler{scheduler}, 
        m_mqttClient{std::move(mqttClient)} 
  {
    
  }

  void Process(const mq::Message &msg) {
    if (msg.to.sys != GetId())
      return;
  }

  [[nodiscard]] mq::Id GetId() const noexcept final {
    return mq::Id::eWeightMeter;
  }
};
