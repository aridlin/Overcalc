#pragma once

#include <memory>
#include <string>

#include "overcalc/ast.hpp"

namespace overcalc {

bool depends_on(const Expr& expr, const std::string& variable);
std::unique_ptr<Expr> derivative(const Expr& expr, const std::string& variable);

}  // namespace overcalc
