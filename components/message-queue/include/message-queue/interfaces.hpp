#pragma once
#include <any>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mq {
enum class Id : std::uint16_t {
  eNone, // use as source id
  eAll,  // use as destination id
  eWeightMeter,
  eLogic
};

struct Addr {
  Id sys;
  std::uint16_t ev;

  [[nodiscard]] constexpr bool operator==(const Addr& other) const noexcept
  {
    return sys == other.sys && ev == other.ev;
  }
  [[nodiscard]] constexpr bool operator!=(const Addr& other) const noexcept
  {
    return !operator==(other);
  }
};
constexpr Addr NONE{Id::eNone, 0};
constexpr Addr BROADCAST{Id::eAll, 0xFFFFU};

/// @brief A structure for passing data between systems
struct Message {
  Addr from{NONE}; ///< The source address
  Addr to{NONE};   ///< The destination address
  std::any data;   ///< Payload
};

/// @brief An interface representing a module of a specific functionality
class ISystem {
public:
  ISystem() noexcept = default;
  ISystem(const ISystem&) = delete;
  ISystem(ISystem&&) = delete;
  ISystem& operator=(const ISystem&) = delete;
  ISystem& operator=(ISystem&&) = delete;
  virtual ~ISystem() = default;
  /// @brief Process a #mq::Message in the message queue
  virtual void Process(const Message&) = 0;
  /// @brief Get a system ID to match the #mq::Message addresses
  [[nodiscard]] virtual Id GetId() const noexcept = 0;

  /// @brief A factory function for creating new instances
  template<typename System, typename... Args,
           typename = std::enable_if_t<std::is_base_of_v<ISystem, System>>>
  [[nodiscard]] static std::shared_ptr<ISystem> Create(Args&&... args)
  {
    // return std::make_unique<System>(std::forward<Args>(args)...);
    return std::make_shared<System>(std::forward<Args>(args)...);
  }
};

class IContext {
protected:
  const unsigned m_numPriorities;

public:
  virtual ~IContext() = default;
  explicit IContext(unsigned numPriorities = 2) : m_numPriorities{numPriorities}
  {
    if (!numPriorities)
      throw std::invalid_argument{"numPriorities must be greater than 0"};
  }
  [[nodiscard]] auto GetNumPriorities() const noexcept
  {
    return m_numPriorities;
  }

  virtual void Push(Message message, unsigned priority) = 0;
  void Push(Message message) { Push(std::move(message), m_numPriorities - 1); }
  virtual void AddSystem(std::shared_ptr<ISystem> system) = 0;
  [[nodiscard]] virtual bool ProcessOneMessage() = 0;

  template<typename Context, typename... Args,
           typename = std::enable_if_t<std::is_base_of_v<IContext, Context>>>
  [[nodiscard]] static std::unique_ptr<IContext> Create(Args&&... args)
  {
    return std::make_unique<Context>(std::forward<Args>(args)...);
  }
};

class IScheduler {
public:
  virtual ~IScheduler() = default;

  template<typename Rep, typename Period>
  void ScheduleAfter(Message message, unsigned priority,
                     const std::chrono::duration<Rep, Period>& timeout)
  {
    Schedule(std::move(message), priority,
             std::chrono::steady_clock::now() + timeout);
  }

  virtual void Schedule(Message message, unsigned priority,
                        std::chrono::steady_clock::time_point when) = 0;

  virtual void ProcessSchedule(mq::IContext& ctx) = 0;

  template<
    typename Scheduler, typename... Args,
    typename = std::enable_if_t<std::is_base_of_v<IScheduler, Scheduler>>>
  [[nodiscard]] static std::unique_ptr<IScheduler> Create(Args&&... args)
  {
    return std::make_unique<Scheduler>(std::forward<Args>(args)...);
  }
};
} // namespace mq
