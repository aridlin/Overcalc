#include "overcalc/format.hpp"
#include <stdexcept>

namespace overcalc {
namespace {
std::string maybe_paren(const Expr& e, const std::string& s) { return dynamic_cast<const Binary*>(&e) ? "(" + s + ")" : s; }
}
std::string to_infix(const Expr& expr) {
  if (auto n = dynamic_cast<const Number*>(&expr)) return std::to_string(n->value);
  if (auto s = dynamic_cast<const Symbol*>(&expr)) return s->name;
  if (auto f = dynamic_cast<const Fraction*>(&expr)) return "(" + to_infix(*f->num) + ")/(" + to_infix(*f->den) + ")";
  if (auto q = dynamic_cast<const Sqrt*>(&expr)) return "sqrt(" + to_infix(*q->radicand) + ")";
  if (auto p = dynamic_cast<const Superscript*>(&expr)) return maybe_paren(*p->base,to_infix(*p->base)) + "^" + maybe_paren(*p->exponent,to_infix(*p->exponent));
  if (auto sb = dynamic_cast<const Subscript*>(&expr)) return to_infix(*sb->base) + "_" + to_infix(*sb->sub);
  auto b = dynamic_cast<const Binary*>(&expr);
  if (!b) throw std::runtime_error("unknown node in infix");
  return maybe_paren(*b->lhs,to_infix(*b->lhs)) + " " + std::string(1,b->op) + " " + maybe_paren(*b->rhs,to_infix(*b->rhs));
}
}
