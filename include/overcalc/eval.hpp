#pragma once
#include <optional>
#include <string>
#include "overcalc/ast.hpp"

namespace overcalc {

struct EvalResult {
  std::string exact;
  std::optional<long double> decimal;
};

EvalResult evaluate(const Expr& expr);
std::string evaluate_to_string(const Expr& expr);

}  // namespace overcalc
