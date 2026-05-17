#include "overcalc/simplify.hpp"

#include "overcalc/format.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace overcalc {
namespace {

const Number* as_number(const std::unique_ptr<Expr>& expr) {
  return dynamic_cast<const Number*>(expr.get());
}

bool is_number(const std::unique_ptr<Expr>& expr, long long value) {
  auto n = as_number(expr);
  return n && n->value == value;
}

long long ipow(long long base, long long exp) {
  long long out = 1;
  for (long long i = 0; i < exp; ++i) out *= base;
  return out;
}

bool same_expr(const std::unique_ptr<Expr>& lhs, const std::unique_ptr<Expr>& rhs) {
  return to_infix(*lhs) == to_infix(*rhs);
}

std::unique_ptr<Expr> neg(std::unique_ptr<Expr> value) {
  return std::make_unique<Binary>('*', std::make_unique<Number>(-1), std::move(value));
}

std::unique_ptr<Expr> simplify_binary(char op, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
  auto ln = as_number(lhs);
  auto rn = as_number(rhs);

  if (op == '+') {
    if (is_number(lhs, 0)) return rhs;
    if (is_number(rhs, 0)) return lhs;
    if (ln && rn) return std::make_unique<Number>(ln->value + rn->value);
  }

  if (op == '-') {
    if (is_number(rhs, 0)) return lhs;
    if (is_number(lhs, 0)) return neg(std::move(rhs));
    if (same_expr(lhs, rhs)) return std::make_unique<Number>(0);
    if (ln && rn) return std::make_unique<Number>(ln->value - rn->value);
  }

  if (op == '*') {
    if (is_number(lhs, 0) || is_number(rhs, 0)) return std::make_unique<Number>(0);
    if (is_number(lhs, 1)) return rhs;
    if (is_number(rhs, 1)) return lhs;
    if (is_number(lhs, -1)) return neg(std::move(rhs));
    if (is_number(rhs, -1)) return neg(std::move(lhs));
    if (ln && rn) return std::make_unique<Number>(ln->value * rn->value);
  }

  if (op == '/') {
    if (is_number(rhs, 0)) return std::make_unique<Binary>(op, std::move(lhs), std::move(rhs));
    if (is_number(lhs, 0)) return std::make_unique<Number>(0);
    if (is_number(rhs, 1)) return lhs;
    if (same_expr(lhs, rhs)) return std::make_unique<Number>(1);
    if (ln && rn && rn->value != 0 && ln->value % rn->value == 0) {
      return std::make_unique<Number>(ln->value / rn->value);
    }
  }

  return std::make_unique<Binary>(op, std::move(lhs), std::move(rhs));
}

std::unique_ptr<Expr> simplify_power(std::unique_ptr<Expr> base, std::unique_ptr<Expr> exponent) {
  if (is_number(exponent, 0)) return std::make_unique<Number>(1);
  if (is_number(exponent, 1)) return base;
  if (is_number(base, 0)) return std::make_unique<Number>(0);
  if (is_number(base, 1)) return std::make_unique<Number>(1);

  auto bn = as_number(base);
  auto en = as_number(exponent);
  if (bn && en && en->value >= 0 && en->value <= 20) {
    return std::make_unique<Number>(ipow(bn->value, en->value));
  }

  return std::make_unique<Superscript>(std::move(base), std::move(exponent));
}

std::unique_ptr<Expr> simplify_sqrt(std::unique_ptr<Expr> radicand, std::unique_ptr<Expr> index) {
  if (auto n = as_number(radicand); n && !index && n->value >= 0) {
    long long root = static_cast<long long>(std::sqrt(static_cast<long double>(n->value)));
    if (root * root == n->value) return std::make_unique<Number>(root);
  }
  if (index) return std::make_unique<Sqrt>(std::move(radicand), std::move(index));
  return std::make_unique<Sqrt>(std::move(radicand));
}

}  // namespace

std::unique_ptr<Expr> clone_expr(const Expr& expr) {
  if (auto n = dynamic_cast<const Number*>(&expr)) return std::make_unique<Number>(n->value);
  if (auto d = dynamic_cast<const Decimal*>(&expr)) return std::make_unique<Decimal>(d->text, d->value);
  if (auto s = dynamic_cast<const Symbol*>(&expr)) return std::make_unique<Symbol>(s->name);
  if (auto fn = dynamic_cast<const FunctionCall*>(&expr)) return std::make_unique<FunctionCall>(fn->name, clone_expr(*fn->arg));
  if (auto b = dynamic_cast<const Binary*>(&expr)) return std::make_unique<Binary>(b->op, clone_expr(*b->lhs), clone_expr(*b->rhs));
  if (auto f = dynamic_cast<const Fraction*>(&expr)) return std::make_unique<Fraction>(clone_expr(*f->num), clone_expr(*f->den));
  if (auto p = dynamic_cast<const Superscript*>(&expr)) return std::make_unique<Superscript>(clone_expr(*p->base), clone_expr(*p->exponent));
  if (auto sub = dynamic_cast<const Subscript*>(&expr)) return std::make_unique<Subscript>(clone_expr(*sub->base), clone_expr(*sub->sub));
  if (auto q = dynamic_cast<const Sqrt*>(&expr)) {
    if (q->index) return std::make_unique<Sqrt>(clone_expr(*q->radicand), clone_expr(*q->index));
    return std::make_unique<Sqrt>(clone_expr(*q->radicand));
  }
  throw std::runtime_error("unknown node in clone");
}

std::unique_ptr<Expr> simplify(const Expr& expr) {
  if (auto n = dynamic_cast<const Number*>(&expr)) return std::make_unique<Number>(n->value);
  if (auto d = dynamic_cast<const Decimal*>(&expr)) return std::make_unique<Decimal>(d->text, d->value);
  if (auto s = dynamic_cast<const Symbol*>(&expr)) return std::make_unique<Symbol>(s->name);
  if (auto fn = dynamic_cast<const FunctionCall*>(&expr)) {
    auto arg = simplify(*fn->arg);
    if (fn->name == "abs") {
      if (auto n = as_number(arg)) return std::make_unique<Number>(n->value < 0 ? -n->value : n->value);
    }
    return std::make_unique<FunctionCall>(fn->name, std::move(arg));
  }
  if (auto b = dynamic_cast<const Binary*>(&expr)) return simplify_binary(b->op, simplify(*b->lhs), simplify(*b->rhs));
  if (auto f = dynamic_cast<const Fraction*>(&expr)) return simplify_binary('/', simplify(*f->num), simplify(*f->den));
  if (auto p = dynamic_cast<const Superscript*>(&expr)) return simplify_power(simplify(*p->base), simplify(*p->exponent));
  if (auto sub = dynamic_cast<const Subscript*>(&expr)) return std::make_unique<Subscript>(simplify(*sub->base), simplify(*sub->sub));
  if (auto q = dynamic_cast<const Sqrt*>(&expr)) {
    auto radicand = simplify(*q->radicand);
    auto index = q->index ? simplify(*q->index) : nullptr;
    return simplify_sqrt(std::move(radicand), std::move(index));
  }
  throw std::runtime_error("unknown node in simplify");
}

}  // namespace overcalc
