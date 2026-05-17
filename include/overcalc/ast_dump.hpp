#pragma once
#include <string>
#include "overcalc/ast.hpp"

namespace overcalc {
std::string ast_to_json(const Expr& expr);
}
