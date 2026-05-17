#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace overcalc {

struct StepLog {
  std::vector<std::string> entries;
  void add(std::string s) { entries.push_back(std::move(s)); }
};

class ParseError final : public std::runtime_error {
 public:
  ParseError(std::string message, std::size_t offset)
      : std::runtime_error(std::move(message)), offset_(offset) {}

  std::size_t offset() const noexcept { return offset_; }

 private:
  std::size_t offset_;
};

} // namespace overcalc
