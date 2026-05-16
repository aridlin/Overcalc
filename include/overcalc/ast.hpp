#pragma once
#include <memory>

namespace overcalc {
struct Expr { virtual ~Expr() = default; };
struct Number final : Expr { long long value; explicit Number(long long v) : value(v) {} };
struct Binary final : Expr {
  char op; std::unique_ptr<Expr> lhs; std::unique_ptr<Expr> rhs;
  Binary(char o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r): op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};
struct Fraction final : Expr {
  std::unique_ptr<Expr> num; std::unique_ptr<Expr> den;
  Fraction(std::unique_ptr<Expr> n, std::unique_ptr<Expr> d): num(std::move(n)), den(std::move(d)) {}
};
}
