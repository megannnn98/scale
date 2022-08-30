#pragma once
#include "message-queue/interfaces.hpp"

#include <algorithm>
#include <esp_log.h>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <utility>
#include <utils/utils.hpp>
#include <vector>

namespace mq {
class Context : public IContext {
  std::vector<std::queue<Message>> m_queues;
  std::vector<std::shared_ptr<ISystem>> m_systems;
  std::mutex m_mutex;

  [[nodiscard]] bool m_TryPop(Message& out)
  {
    std::scoped_lock lock{m_mutex};
    for (auto& queue : m_queues)
      if (!queue.empty()) {
        out = std::move(queue.front());
        queue.pop();
        return true;
      }
    return false;
  }

public:
  explicit Context(unsigned numPriorities = 2)
    : IContext(numPriorities), m_queues(m_numPriorities)
  {
  }

  void Push(Message message, unsigned priority) override
  {
    ESP_LOGV("CTX", "Push: sys %d, ev %d | sys %d, ev %d | has value %d",
             utils::EnumValue(message.from.sys), message.from.ev,
             utils::EnumValue(message.to.sys), message.to.ev,
             message.data.has_value());
    std::scoped_lock lock{m_mutex};
    m_queues.at(priority).push(std::move(message));
  }

  void AddSystem(std::shared_ptr<ISystem> system) override
  {
    if (!system)
      throw std::invalid_argument{"the system can not be null"};
    m_systems.push_back(system);
  }

  [[nodiscard]] bool ProcessOneMessage() override
  {
    Message message{};
    auto ret = m_TryPop(message);
    if (ret && message.to.sys != Id::eNone) {
      ESP_LOGV("CTX", "Process: sys %d, ev %d | sys %d, ev %d | has value %d",
               utils::EnumValue(message.from.sys), message.from.ev,
               utils::EnumValue(message.to.sys), message.to.ev,
               message.data.has_value());

      std::for_each(m_systems.begin(), m_systems.end(),
                    [&message](auto& system) { system->Process(message); });
    }
    return ret;
  }
};
} // namespace mq
