#pragma once
#include "message-queue/interfaces.hpp"

#include <algorithm>
#include <chrono>
#include <esp_log.h>
#include <mutex>
#include <set>
#include <utils/utils.hpp>

namespace mq {
class Scheduler : public IScheduler {
  struct ScheduledMessage {
    Message message;
    unsigned priority;
    std::chrono::steady_clock::time_point when;
  };

  struct Comparator {
    using is_transparent = void;

    [[nodiscard]] bool operator()(const ScheduledMessage& l,
                                  const ScheduledMessage& r) const noexcept
    {
      return l.when < r.when;
    }

    [[nodiscard]] bool operator()(
      const ScheduledMessage& l,
      const decltype(ScheduledMessage::when)& when) const noexcept
    {
      return l.when < when;
    }

    [[nodiscard]] bool operator()(const decltype(ScheduledMessage::when)& when,
                                  const ScheduledMessage& r) const noexcept
    {
      return when < r.when;
    }
  };

  std::multiset<ScheduledMessage, Comparator> m_messages;
  std::mutex m_mutex;

public:
  void Schedule(Message message, unsigned priority,
                std::chrono::steady_clock::time_point when) override
  {
    ESP_LOGV("SCHEDULER",
             "Planing: sys %d, ev %d | sys %d, ev %d | has value %d",
             utils::EnumValue(message.from.sys), message.from.ev,
             utils::EnumValue(message.to.sys), message.to.ev,
             message.data.has_value());
    std::scoped_lock lock{m_mutex};
    m_messages.insert(ScheduledMessage{std::move(message), priority, when});
  };

  void ProcessSchedule(mq::IContext& ctx) override
  {
    std::scoped_lock lock{m_mutex};
    auto first = m_messages.begin();
    auto last = m_messages.upper_bound(std::chrono::steady_clock::now());
    auto it = first;
    try {
      for (; it != last; std::advance(it, 1)) {
        auto message{it->message};
        ESP_LOGV("SCHEDULER",
                 "Execution: sys %d, ev %d | sys %d, ev %d | has value %d",
                 utils::EnumValue(message.from.sys), message.from.ev,
                 utils::EnumValue(message.to.sys), message.to.ev,
                 message.data.has_value());
        ctx.Push(std::move(message), it->priority);
      }
    }
    catch (...) {
      m_messages.erase(first, std::next(it));
      throw;
    }
    m_messages.erase(first, last);
  }
};
} // namespace mq
