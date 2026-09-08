#pragma once
#include <cstddef>
#include <memory>
#include <string>

namespace overcalc {

inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

struct Expr {
  std::size_t start = npos;
  std::size_t end = npos;
  virtual ~Expr() = default;
};

struct Number final : Expr {
  long long value;
  explicit Number(long long v) : value(v) {}
};

struct Decimal final : Expr {
  std::string text;
  long double value;
  Decimal(std::string t, long double v) : text(std::move(t)), value(v) {}
};

struct Symbol final : Expr {
  std::string name;
  explicit Symbol(std::string n) : name(std::move(n)) {}
};

struct FunctionCall final : Expr {
  std::string name;
  std::unique_ptr<Expr> arg;
  FunctionCall(std::string n, std::unique_ptr<Expr> a)
      : name(std::move(n)), arg(std::move(a)) {}
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
  std::unique_ptr<Expr> index;
  explicit Sqrt(std::unique_ptr<Expr> r) : radicand(std::move(r)) {}
  Sqrt(std::unique_ptr<Expr> r, std::unique_ptr<Expr> i)
      : radicand(std::move(r)), index(std::move(i)) {}
};

}  // namespace overcalc
