#pragma once
#include <string>
#include "overcalc/ast.hpp"

namespace overcalc {

std::string to_latex(const Expr& expr);

}  // namespace overcalc
