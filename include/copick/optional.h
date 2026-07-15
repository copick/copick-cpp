// A minimal C++11 optional, since the public headers cannot use std::optional (C++17).
//
// Only supports default-constructible, copyable value types (which is all copick uses:
// int, double, std::string, std::array). Deliberately tiny — the reflect-cpp DTO layer
// in src/ maps this to std::optional for JSON.
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_OPTIONAL_H
#define COPICK_OPTIONAL_H

#include <stdexcept>

namespace copick {

template <class T>
class optional {
 public:
  optional() {}
  optional(const T& value) : has_(true), value_(value) {}  // NOLINT(runtime/explicit)

  optional& operator=(const T& value) {
    has_ = true;
    value_ = value;
    return *this;
  }

  bool has_value() const { return has_; }
  explicit operator bool() const { return has_; }

  const T& value() const {
    if (!has_) throw std::runtime_error("optional: bad access");
    return value_;
  }
  T& value() {
    if (!has_) throw std::runtime_error("optional: bad access");
    return value_;
  }

  const T& operator*() const { return value_; }
  T& operator*() { return value_; }

  T value_or(const T& fallback) const { return has_ ? value_ : fallback; }

  void reset() {
    has_ = false;
    value_ = T();
  }

 private:
  bool has_ = false;
  T value_ = T();
};

template <class T>
bool operator==(const optional<T>& a, const optional<T>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a.has_value() || (*a == *b);
}

template <class T>
bool operator!=(const optional<T>& a, const optional<T>& b) {
  return !(a == b);
}

}  // namespace copick

#endif  // COPICK_OPTIONAL_H
