#include "overcalc/derivative.hpp"

#include "overcalc/simplify.hpp"

#include <stdexcept>
#include <utility>

namespace overcalc {
namespace {

std::unique_ptr<Expr> num(long long value) {
  return std::make_unique<Number>(value);
}

std::unique_ptr<Expr> add(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
  return std::make_unique<Binary>('+', std::move(lhs), std::move(rhs));
}

std::unique_ptr<Expr> sub(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
  return std::make_unique<Binary>('-', std::move(lhs), std::move(rhs));
}

std::unique_ptr<Expr> mul(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
  return std::make_unique<Binary>('*', std::move(lhs), std::move(rhs));
}

std::unique_ptr<Expr> div(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
  return std::make_unique<Binary>('/', std::move(lhs), std::move(rhs));
}

std::unique_ptr<Expr> pow_expr(std::unique_ptr<Expr> base, std::unique_ptr<Expr> exponent) {
  return std::make_unique<Superscript>(std::move(base), std::move(exponent));
}

std::unique_ptr<Expr> fn(std::string name, std::unique_ptr<Expr> arg) {
  return std::make_unique<FunctionCall>(std::move(name), std::move(arg));
}

std::unique_ptr<Expr> neg(std::unique_ptr<Expr> expr) {
  return mul(num(-1), std::move(expr));
}

}  // namespace

bool depends_on(const Expr& expr, const std::string& variable) {
  if (auto s = dynamic_cast<const Symbol*>(&expr)) return s->name == variable;
  if (dynamic_cast<const Number*>(&expr) || dynamic_cast<const Decimal*>(&expr)) return false;
  if (auto fn = dynamic_cast<const FunctionCall*>(&expr)) return depends_on(*fn->arg, variable);
  if (auto b = dynamic_cast<const Binary*>(&expr)) return depends_on(*b->lhs, variable) || depends_on(*b->rhs, variable);
  if (auto f = dynamic_cast<const Fraction*>(&expr)) return depends_on(*f->num, variable) || depends_on(*f->den, variable);
  if (auto p = dynamic_cast<const Superscript*>(&expr)) return depends_on(*p->base, variable) || depends_on(*p->exponent, variable);
  if (auto sub = dynamic_cast<const Subscript*>(&expr)) return depends_on(*sub->base, variable) || depends_on(*sub->sub, variable);
  if (auto q = dynamic_cast<const Sqrt*>(&expr)) return depends_on(*q->radicand, variable) || (q->index && depends_on(*q->index, variable));
  return false;
}

std::unique_ptr<Expr> derivative(const Expr& expr, const std::string& variable) {
  if (dynamic_cast<const Number*>(&expr) || dynamic_cast<const Decimal*>(&expr)) return num(0);
  if (auto s = dynamic_cast<const Symbol*>(&expr)) return num(s->name == variable ? 1 : 0);

  if (auto fnc = dynamic_cast<const FunctionCall*>(&expr)) {
    auto inner_prime = derivative(*fnc->arg, variable);
    if (fnc->name == "sin") return simplify(*mul(fn("cos", clone_expr(*fnc->arg)), std::move(inner_prime)));
    if (fnc->name == "cos") return simplify(*mul(neg(fn("sin", clone_expr(*fnc->arg))), std::move(inner_prime)));
    if (fnc->name == "tan") {
      return simplify(*mul(div(num(1), pow_expr(fn("cos", clone_expr(*fnc->arg)), num(2))), std::move(inner_prime)));
    }
    if (fnc->name == "ln" || fnc->name == "log") return simplify(*div(std::move(inner_prime), clone_expr(*fnc->arg)));
    if (fnc->name == "exp") return simplify(*mul(fn("exp", clone_expr(*fnc->arg)), std::move(inner_prime)));
    if (fnc->name == "percent") return simplify(*div(std::move(inner_prime), num(100)));
    if (fnc->name == "abs" || fnc->name == "fact") {
      throw std::runtime_error("unsupported derivative for function: " + fnc->name);
    }
    throw std::runtime_error("unsupported derivative for function: " + fnc->name);
  }

  if (auto b = dynamic_cast<const Binary*>(&expr)) {
    if (b->op == '+') return simplify(*add(derivative(*b->lhs, variable), derivative(*b->rhs, variable)));
    if (b->op == '-') return simplify(*sub(derivative(*b->lhs, variable), derivative(*b->rhs, variable)));
    if (b->op == '*') {
      return simplify(*add(mul(derivative(*b->lhs, variable), clone_expr(*b->rhs)),
                           mul(clone_expr(*b->lhs), derivative(*b->rhs, variable))));
    }
    if (b->op == '/') {
      return simplify(*div(sub(mul(derivative(*b->lhs, variable), clone_expr(*b->rhs)),
                               mul(clone_expr(*b->lhs), derivative(*b->rhs, variable))),
                           pow_expr(clone_expr(*b->rhs), num(2))));
    }
  }

  if (auto f = dynamic_cast<const Fraction*>(&expr)) {
    auto as_binary = std::make_unique<Binary>('/', clone_expr(*f->num), clone_expr(*f->den));
    return derivative(*as_binary, variable);
  }

  if (auto p = dynamic_cast<const Superscript*>(&expr)) {
    if (auto exponent = dynamic_cast<const Number*>(p->exponent.get())) {
      return simplify(*mul(mul(num(exponent->value), pow_expr(clone_expr(*p->base), num(exponent->value - 1))),
                           derivative(*p->base, variable)));
    }
    if (!depends_on(*p->base, variable)) {
      return simplify(*mul(mul(clone_expr(expr), fn("ln", clone_expr(*p->base))),
                           derivative(*p->exponent, variable)));
    }
    throw std::runtime_error("unsupported derivative for non-constant exponent");
  }

  if (auto q = dynamic_cast<const Sqrt*>(&expr)) {
    if (q->index) throw std::runtime_error("unsupported derivative for indexed root");
    return simplify(*div(derivative(*q->radicand, variable), mul(num(2), std::make_unique<Sqrt>(clone_expr(*q->radicand)))));
  }

  if (auto subscript = dynamic_cast<const Subscript*>(&expr)) {
    return depends_on(*subscript, variable) ? num(1) : num(0);
  }

  throw std::runtime_error("unsupported derivative node");
}

}  // namespace overcalc
