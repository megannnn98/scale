#include "utils/hexstrconv.hpp"

#include <algorithm>
#include <charconv>
#include <gsl/span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::uint8_t> hexstrconv::FromHexStr(std::string_view in)
{
  constexpr auto step{2U};
  std::vector<std::uint8_t> result;
  result.reserve(in.size() / 2);

  if (in.size() % 2 != 0)
    throw std::invalid_argument{"the input length is odd"};

  constexpr auto Str2Byte = [](std::string_view str) {
    std::uint8_t res{};
    if (auto [ptr, ec]{
          std::from_chars(str.data(), std::next(str.data(), step), res, 16)};
        ec != std::errc())
      throw std::invalid_argument{"non-hex chars"};
    return res;
  };

  for (std::string_view::size_type i{}; i < in.size(); i += step)
    result.push_back(Str2Byte(in.substr(i, step)));

  return result;
}

std::string hexstrconv::ToHexStr(gsl::span<const std::uint8_t> in)
{
  std::string result;
  result.reserve(in.size() * 2);

  std::for_each(in.begin(), in.end(), [&result](std::uint8_t byte) {
    std::array<char, 2> out{'0', '0'};
    if (auto [p, ec] =
          std::to_chars(std::next(out.data(), byte > 0x0FU ? 0 : 1),
                        std::next(out.data(), out.size()), byte, 16);
        ec != std::errc{})
      throw std::runtime_error{"conversion failed"};
    result.append(out.cbegin(), out.cend());
  });

  return result;
}
