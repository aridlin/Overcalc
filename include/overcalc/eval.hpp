#pragma once
#include <optional>
#include <string>
#include "overcalc/ast.hpp"
#include "overcalc/diagnostics.hpp"

namespace overcalc {

struct EvalResult {
  std::string exact;
  std::optional<long double> decimal;
};

EvalResult evaluate(const Expr& expr, StepLog* steps = nullptr);
std::string evaluate_to_string(const Expr& expr);

}  // namespace overcalc
