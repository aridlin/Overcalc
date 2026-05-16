#pragma once
#include <memory>
#include <string>

namespace overcalc {

struct Expr { virtual ~Expr() = default; };

struct Number final : Expr {
  long long value;
  explicit Number(long long v) : value(v) {}
};

struct Symbol final : Expr {
  std::string name;
  explicit Symbol(std::string n) : name(std::move(n)) {}
};

struct Binary final : Expr {
  char op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
  Binary(char o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
      : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct Fraction final : Expr {
  std::unique_ptr<Expr> num;
  std::unique_ptr<Expr> den;
  Fraction(std::unique_ptr<Expr> n, std::unique_ptr<Expr> d)
      : num(std::move(n)), den(std::move(d)) {}
};

struct Superscript final : Expr {
  std::unique_ptr<Expr> base;
  std::unique_ptr<Expr> exponent;
  Superscript(std::unique_ptr<Expr> b, std::unique_ptr<Expr> e)
      : base(std::move(b)), exponent(std::move(e)) {}
};

struct Subscript final : Expr {
  std::unique_ptr<Expr> base;
  std::unique_ptr<Expr> sub;
  Subscript(std::unique_ptr<Expr> b, std::unique_ptr<Expr> s)
      : base(std::move(b)), sub(std::move(s)) {}
};

struct Sqrt final : Expr {
  std::unique_ptr<Expr> radicand;
  explicit Sqrt(std::unique_ptr<Expr> r) : radicand(std::move(r)) {}
};

}  // namespace overcalc
