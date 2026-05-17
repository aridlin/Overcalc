#include "overcalc/latex.hpp"

#include <stdexcept>

namespace overcalc {
namespace {

std::string wrap_if_binary(const Expr& e, const std::string& out) {
  return dynamic_cast<const Binary*>(&e) ? "(" + out + ")" : out;
}

}  // namespace

std::string to_latex(const Expr& expr) {
  if (auto n = dynamic_cast<const Number*>(&expr)) return std::to_string(n->value);
  if (auto s = dynamic_cast<const Symbol*>(&expr)) return s->name;
  if (auto f = dynamic_cast<const Fraction*>(&expr)) return "\\frac{" + to_latex(*f->num) + "}{" + to_latex(*f->den) + "}";
  if (auto q = dynamic_cast<const Sqrt*>(&expr)) return "\\sqrt{" + to_latex(*q->radicand) + "}";
  if (auto p = dynamic_cast<const Superscript*>(&expr)) return wrap_if_binary(*p->base, to_latex(*p->base)) + "^{" + to_latex(*p->exponent) + "}";
  if (auto sb = dynamic_cast<const Subscript*>(&expr)) return wrap_if_binary(*sb->base, to_latex(*sb->base)) + "_{" + to_latex(*sb->sub) + "}";
  auto b = dynamic_cast<const Binary*>(&expr);
  if (!b) throw std::runtime_error("unknown node in latex serialization");
  return wrap_if_binary(*b->lhs, to_latex(*b->lhs)) + std::string(1, b->op) + wrap_if_binary(*b->rhs, to_latex(*b->rhs));
}

}  // namespace overcalc
