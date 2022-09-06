#pragma once

class Filter {

  const float koeff_ = 1.f;
  float prev_{};
public:
  
  explicit Filter(const float koeff, const float init)
  : koeff_{koeff}, prev_{init}
  {
  }

  Filter() = delete;
  Filter(const Filter&) = delete;
  Filter& operator=(const Filter&) = delete;
  Filter(Filter&&) = delete;
  Filter& operator=(Filter&&) = delete;
  ~Filter() = default;

  [[nodiscard]] float filter(const float val) noexcept
  {
    const float tmp{prev_};
    prev_ = val;    
    return val*(koeff_) + tmp*(1.f - koeff_);
  }

};
