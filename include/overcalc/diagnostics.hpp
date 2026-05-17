#pragma once
#include <string>
#include <vector>

namespace overcalc {

struct StepLog {
  std::vector<std::string> entries;
  void add(std::string s) { entries.push_back(std::move(s)); }
};

} // namespace overcalc
