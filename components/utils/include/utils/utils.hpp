#pragma once
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace utils {
/// @brief Convert an enumerator to its underlying value
///
/// @param value An enumerator
/// @return underlying numerical value of the enumerator
template<typename Enum, typename = std::enable_if_t<std::is_enum_v<Enum>>>
[[nodiscard]] constexpr auto EnumValue(Enum value) noexcept
{
  return static_cast<std::underlying_type_t<Enum>>(value);
}

// helper type for the visitor
// https://arne-mertz.de/2018/05/overload-build-a-variant-visitor-on-the-fly/
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0051r3.pdf
template<class... Fs>
struct overload : Fs... {
// NOTE: current GCC doesn't support this type of CTAD
#if 0
  template<class... Ts>
  overload(Ts&&... ts) : Fs{ std::forward<Ts>(ts) }...
  {}
#endif

  using Fs::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template<class... Ts>
overload(Ts&&...) -> overload<std::remove_reference_t<Ts>...>;

/// @brief Assemble an unsigned integer from individual bytes LSB-first
///
/// @tparam ReturnType The type of an integer to assemble
/// @param first The least significant byte
/// @param args A parameter pack with the rest of the bytes
template<typename ReturnType, typename... Args,
         typename = std::enable_if_t<
           (sizeof(ReturnType) > sizeof...(Args)) &&
           std::is_integral_v<ReturnType> && std::is_unsigned_v<ReturnType> &&
           !std::is_same_v<bool, std::remove_cv_t<ReturnType>> &&
           (... && std::is_same_v<std::uint8_t, Args>)>>
[[nodiscard]] constexpr ReturnType AssembleBytes(std::uint8_t first,
                                                 Args... args) noexcept
{
  if constexpr (sizeof...(Args) > 0U)
    return static_cast<ReturnType>(
      first | AssembleBytes<ReturnType>(args...) << 8U);
  return first;
}
static_assert(AssembleBytes<std::uint16_t>(std::uint8_t{0x12U},
                                           std::uint8_t{0x34U}) == 0x3412U);

/// @brief Extract a single byte from an unsigned integer by its index
///
/// @tparam INDEX A zero-based index of a byte to extract starting from
/// the least significant byte
/// @param value An integer value to extract a byte from it
/// @return A byte at the specified index of the value param
template<
  std::size_t INDEX, typename Value,
  typename = std::enable_if_t<
    std::is_integral_v<Value> && std::is_unsigned_v<Value> &&
    !std::is_same_v<bool, std::remove_cv_t<Value>> && (sizeof(Value) > INDEX)>>
[[nodiscard]] constexpr std::uint8_t GetByteByIndex(Value value) noexcept
{
  return static_cast<std::uint8_t>(Value{0xFFU} & (value >> 8U * INDEX));
}
static_assert(GetByteByIndex<0>(0x0123456789ABCDEFU) == 0xEFU);
static_assert(GetByteByIndex<1>(0x0123456789ABCDEFU) == 0xCDU);
static_assert(GetByteByIndex<2>(0x0123456789ABCDEFU) == 0xABU);
static_assert(GetByteByIndex<3>(0x0123456789ABCDEFU) == 0x89U);
static_assert(GetByteByIndex<4>(0x0123456789ABCDEFU) == 0x67U);
static_assert(GetByteByIndex<5>(0x0123456789ABCDEFU) == 0x45U);
static_assert(GetByteByIndex<6>(0x0123456789ABCDEFU) == 0x23U);
static_assert(GetByteByIndex<7>(0x0123456789ABCDEFU) == 0x01U);

template<typename Key, typename Value, std::size_t size>
struct ConstMap {
  std::array<std::pair<Key, Value>, size> data;

  [[nodiscard]] constexpr Value at(const Key& key) const
  { // NOLINT
    if (const auto it =
          std::find_if(data.cbegin(), data.cend(),
                       [&key](const auto& v) { return v.first == key; });
        it != data.cend()) {
      return it->second;
    }
    throw std::range_error{"Key not found in ConstMap"};
  }
};

template<typename Key, typename Value, std::size_t size>
ConstMap(std::array<std::pair<Key, Value>, size>) -> ConstMap<Key, Value, size>;

} // namespace utils
