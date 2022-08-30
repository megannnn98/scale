#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <esp_log.h>
#include <functional>
#include <memory>
#include <mqtt_client.h>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace mqtt {
constexpr bool MatchTopic(std::string_view filter,
                          std::string_view topic) noexcept
{
  using namespace std::literals::string_view_literals;

  if (filter.empty() && topic.empty())
    return true;

  if (filter.empty() || topic.empty())
    return false;

  if (filter == "#"sv)
    return topic.front() != '$';

  if (filter.front() == '+' && topic.front() == '$')
    return false;

  if (auto plus = filter.find('+'), slash = topic.find('/', plus);
      plus != std::string_view::npos &&
      topic.substr(0, plus) == filter.substr(0, plus))
    return MatchTopic(plus < filter.length() ? filter.substr(plus + 1) : ""sv,
                      slash < topic.length() ? topic.substr(slash) : ""sv);

  if (filter.back() == '#')
    return topic.substr(0, filter.length() - 2) ==
           filter.substr(0, filter.length() - 2);

  return filter == topic;
}
// some tests according to
// http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718106
static_assert(MatchTopic({}, {}));
static_assert(!MatchTopic({}, "a"));
static_assert(!MatchTopic("a", {}));
static_assert(MatchTopic("/", "/"));
static_assert(MatchTopic("/aaa", "/aaa"));
static_assert(!MatchTopic("/aaa", "/bbb"));
static_assert(MatchTopic("/aaa/bbb", "/aaa/bbb"));
static_assert(!MatchTopic("/aaa/bbb", "/aaa/ccc"));
static_assert(MatchTopic("/aaa/+/ccc", "/aaa/bbb/ccc"));
static_assert(!MatchTopic("/aaa/+/bbb", "/aaa/bbb/ccc"));
static_assert(MatchTopic("/aaa/bbb/#", "/aaa/bbb/ccc"));
static_assert(!MatchTopic("/aaa/ddd/#", "/aaa/bbb/ccc"));
static_assert(MatchTopic("/aaa/bbb/#", "/aaa/bbb/ccc/ddd/eee"));
static_assert(MatchTopic("/aaa/+/+/ddd/#", "/aaa/bbb/ccc/ddd/eee/fff"));
static_assert(MatchTopic("/aaa/#", "/aaa"));
static_assert(MatchTopic("/aaa/#", "/aaa/"));
static_assert(MatchTopic("/aaa/+", "/aaa/"));
static_assert(MatchTopic("sport/tennis/player1/#", "sport/tennis/player1"));
static_assert(MatchTopic("sport/tennis/player1/#",
                         "sport/tennis/player1/ranking"));
static_assert(MatchTopic("sport/tennis/player1/#",
                         "sport/tennis/player1/score/wimbledon"));
static_assert(MatchTopic("#", "sport/tennis/player1/score/wimbledon"));
static_assert(!MatchTopic("#", "$sport/tennis/player1/score/wimbledon"));
static_assert(MatchTopic("sport/tennis/+", "sport/tennis/player1"));
static_assert(MatchTopic("sport/tennis/+", "sport/tennis/player2"));
static_assert(!MatchTopic("sport/tennis/+", "sport/tennis/player1/ranking"));
static_assert(MatchTopic("sport/+", "sport/"));
static_assert(!MatchTopic("sport/+", "sport"));
static_assert(MatchTopic("+/+", "/finance"));
static_assert(MatchTopic("/+", "/finance"));
static_assert(!MatchTopic("+", "/finance"));
static_assert(!MatchTopic("+/monitor/Clients", "$SYS/monitor/Clients"));
static_assert(MatchTopic("$SYS/monitor/+", "$SYS/monitor/Clients"));

enum class QoS : std::uint8_t {
  e0,
  e1,
};

class Client {
public:
  static constexpr int ERROR_ID = -1;
  static constexpr int DEFAULT_BUFFER_SIZE = 8192;
  static constexpr int KEEP_ALIVE = 30; // seconds

  Client(std::string_view url, std::string_view clientId,
         std::string_view rootCert, std::string_view clientCert,
         std::string_view privateKey,
         std::function<void(void)> connectCallback = nullptr,
         std::function<void(void)> disconnectCallback = nullptr,
         unsigned bufferSize = DEFAULT_BUFFER_SIZE)
    : m_pClient{nullptr, &esp_mqtt_client_destroy}
    , m_connectCallback{connectCallback}
    , m_disconnectCallback{disconnectCallback}
  {
    esp_mqtt_client_config_t mqttCfg{};
    mqttCfg.uri = url.data();
    mqttCfg.client_id = clientId.data();
    mqttCfg.cert_pem = rootCert.data();
    mqttCfg.client_cert_pem = clientCert.data();
    mqttCfg.client_key_pem = privateKey.data();
    mqttCfg.event_handle = m_MqttEventHandler;
    mqttCfg.buffer_size = bufferSize;
    mqttCfg.keepalive = KEEP_ALIVE;
    mqttCfg.user_context = this;

    if (esp_mqtt_client_handle_t pClient = esp_mqtt_client_init(&mqttCfg);
        pClient)
      m_pClient.reset(pClient);
    else
      throw std::runtime_error{"Failed to connect to the MQTT broker"};

    esp_mqtt_client_start(m_pClient.get());
  }
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  Client(Client&&) = delete;
  Client& operator=(Client&&) = delete;
  ~Client() = default;

  bool AddFilterHandler(
    std::string_view topicFilter,
    std::function<void(std::string&&, std::string&&)> callback)
  {
    std::scoped_lock lock{m_topicHandlersMutex};
    return m_topicHandlers.emplace(topicFilter, std::move(callback)).second;
  }

  void RemoveFilterHandler(std::string_view topicFilter)
  {
    std::scoped_lock lock{m_topicHandlersMutex};
    m_topicHandlers.erase(std::string{topicFilter});
  }

  int Subscribe(std::string_view topicFilter, QoS qos)
  {
    {
      std::scoped_lock lock{m_subscriptionListMutex};
      m_subscriptionList.emplace(std::string{topicFilter}, qos);
    }
    ESP_LOGI(TAG, "Subscribing to '%s'", topicFilter.data());
    return m_isConnected.load(std::memory_order_relaxed)
             ? esp_mqtt_client_subscribe(
                 m_pClient.get(), topicFilter.data(), qos == QoS::e0 ? 0 : 1)
             : ERROR_ID;
  }

  int Unsubscribe(std::string_view topic)
  {
    {
      std::scoped_lock lock{m_subscriptionListMutex};
      m_subscriptionList.erase(std::string{topic});
    }
    return m_isConnected.load(std::memory_order_relaxed)
             ? esp_mqtt_client_unsubscribe(m_pClient.get(), topic.data())
             : ERROR_ID;
  }

  int Publish(std::string_view topic, std::string_view data, QoS qos,
              bool retain = 0)
  {
    return m_isConnected.load(std::memory_order_relaxed)
             ? esp_mqtt_client_publish(
                 m_pClient.get(), topic.data(), data.data(), data.size(),
                 qos == QoS::e0 ? 0 : 1, retain)
             : ERROR_ID;
  }

  int PublishAsync(std::string_view topic, std::string_view data, QoS qos,
                   bool retain = false, bool store = false)
  {
    return m_isConnected.load(std::memory_order_relaxed)
             ? esp_mqtt_client_enqueue(
                 m_pClient.get(), topic.data(), data.data(), data.size(),
                 qos == QoS::e0 ? 0 : 1, retain ? 1 : 0, store)
             : ERROR_ID;
  }

private:
  static constexpr const char* TAG = "MQTT";

  // topicFilter -> topicHandler(topic, payload)
  std::unordered_map<std::string,
                     std::function<void(std::string&&, std::string&&)>>
    m_topicHandlers;
  std::mutex m_topicHandlersMutex;

  std::pair<std::string, std::string> m_messageInFlight; // {topic, payload}

  std::mutex m_subscriptionListMutex;
  std::unordered_map<std::string, QoS> m_subscriptionList;

  std::unique_ptr<std::remove_pointer_t<esp_mqtt_client_handle_t>,
                  decltype(&esp_mqtt_client_destroy)>
    m_pClient;

  std::function<void(void)> m_connectCallback{};
  std::function<void(void)> m_disconnectCallback{};

  std::atomic_bool m_isConnected{};

  static esp_err_t m_MqttEventHandler(esp_mqtt_event_handle_t pEvent)
  {
    auto& self = *static_cast<Client*>(pEvent->user_context);

    switch (pEvent->event_id) {
    case MQTT_EVENT_BEFORE_CONNECT: ESP_LOGD(TAG, "BEFORE_CONNECT"); break;
    case MQTT_EVENT_CONNECTED: {
      ESP_LOGI(TAG, "CONNECTED");
      self.m_isConnected.store(true, std::memory_order_relaxed);
      std::scoped_lock lock{self.m_subscriptionListMutex};
      for (const auto& [topic, qos] : self.m_subscriptionList)
        esp_mqtt_client_subscribe(
          self.m_pClient.get(), topic.data(), qos == QoS::e0 ? 0 : 1);
      if (self.m_connectCallback) {
        self.m_connectCallback();
      }
    } break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "DISCONNECTED");
      self.m_isConnected.store(false, std::memory_order_relaxed);
      if (self.m_disconnectCallback) {
        self.m_disconnectCallback();
      }
      break;
    case MQTT_EVENT_SUBSCRIBED: ESP_LOGD(TAG, "SUBSCRIBED"); break;
    case MQTT_EVENT_UNSUBSCRIBED: ESP_LOGD(TAG, "UNSUBSCRIBED"); break;
    case MQTT_EVENT_PUBLISHED: ESP_LOGD(TAG, "PUBLISHED"); break;
    case MQTT_EVENT_DATA: {
      ESP_LOGD(TAG, "DATA");
      auto [topic, payload] = self.m_messageInFlight;

      // the first message block event includes the topic
      if (pEvent->topic) {
        topic =
          std::string{pEvent->topic,
                      static_cast<std::string::size_type>(pEvent->topic_len)};
        payload.reserve(pEvent->total_data_len);
      }

      payload.append(pEvent->data,
                     static_cast<std::string::size_type>(pEvent->data_len));

      if (static_cast<int>(payload.length()) == pEvent->total_data_len) {
        std::scoped_lock lock{self.m_topicHandlersMutex};
        // see https://dev.to/maniowy/capture-structured-bindings-in-c-17-4hlb
        if (auto it = std::find_if(
              self.m_topicHandlers.begin(), self.m_topicHandlers.end(),
              [&topic = topic](const auto& element) {
                return MatchTopic(element.first, topic);
              });
            it != self.m_topicHandlers.end())
          it->second(std::move(topic), std::move(payload));
        else
          ESP_LOGD(TAG, "A message from %.*s topic is ignored",
                   pEvent->topic_len, pEvent->topic);
      }
    } break;
    case MQTT_EVENT_ERROR:
      ESP_LOGD(TAG, "MQTT_EVENT_ERROR");
      if (pEvent->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        ESP_LOGD(TAG, "Last error code reported from esp-tls: 0x%x",
                 pEvent->error_handle->esp_tls_last_esp_err);
        ESP_LOGD(TAG, "Last tls stack error number: 0x%x",
                 pEvent->error_handle->esp_tls_stack_err);
        ESP_LOGD(TAG, "Last captured errno : %d (%s)",
                 pEvent->error_handle->esp_transport_sock_errno,
                 std::strerror(pEvent->error_handle->esp_transport_sock_errno));
      }
      else if (pEvent->error_handle->error_type ==
               MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        ESP_LOGW(TAG, "Connection refused error: 0x%x",
                 pEvent->error_handle->connect_return_code);
      else
        ESP_LOGW(TAG, "Unknown error type: 0x%x",
                 pEvent->error_handle->error_type);
      break;
    default: ESP_LOGD(TAG, "Other event: %d", pEvent->event_id); break;
    }

    return ESP_OK;
  }
};
} // namespace mqtt
