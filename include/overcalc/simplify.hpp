#pragma once

#include <memory>

#include "overcalc/ast.hpp"

namespace overcalc {

std::unique_ptr<Expr> clone_expr(const Expr& expr);
std::unique_ptr<Expr> simplify(const Expr& expr);

}  // namespace overcalc
