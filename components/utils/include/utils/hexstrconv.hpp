#pragma once

#include <gsl/span>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hexstrconv {
[[nodiscard]] std::vector<std::uint8_t> FromHexStr(std::string_view in);
[[nodiscard]] std::string ToHexStr(gsl::span<const std::uint8_t> in);
template<typename T>
[[nodiscard]] std::string IntToHexStr(T val, std::size_t width = sizeof(T) * 2)
{
  std::stringstream ss;
  ss << std::setfill('0') << std::uppercase << std::setw(width) << std::hex
     << (val | 0);
  return ss.str();
}
} // namespace hexstrconv
