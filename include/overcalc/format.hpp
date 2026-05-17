#pragma once
#include <string>
#include "overcalc/ast.hpp"

namespace overcalc {
std::string to_infix(const Expr& expr);
}
