#include "overcalc/parser.hpp"
#include <cctype>
#include <stdexcept>

namespace overcalc {
namespace {

struct P {
  std::string s;
  std::size_t i = 0;
  explicit P(std::string in) : s(std::move(in)) {}

  void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
  bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }

  std::unique_ptr<Expr> number() {
    ws();
    if (i >= s.size() || !std::isdigit((unsigned char)s[i])) throw std::runtime_error("expected number");
    long long v = 0;
    while (i < s.size() && std::isdigit((unsigned char)s[i])) v = v * 10 + (s[i++] - '0');
    return std::make_unique<Number>(v);
  }

  std::unique_ptr<Expr> factor() {
    if (eat('(')) { auto e = expr(); if (!eat(')')) throw std::runtime_error("missing )"); return e; }
    return number();
  }

  std::unique_ptr<Expr> term() {
    auto l = factor();
    while (true) {
      ws();
      if (i >= s.size() || (s[i] != '*' && s[i] != '/')) break;
      char op = s[i++];
      auto r = factor();
      l = std::make_unique<Binary>(op, std::move(l), std::move(r));
    }
    return l;
  }

  std::unique_ptr<Expr> expr() {
    auto l = term();
    while (true) {
      ws();
      if (i >= s.size() || (s[i] != '+' && s[i] != '-')) break;
      char op = s[i++];
      auto r = term();
      l = std::make_unique<Binary>(op, std::move(l), std::move(r));
    }
    return l;
  }
};

std::unique_ptr<Expr> parse_frac(const std::string& in) {
  auto lb1 = in.find('{'); auto rb1 = in.find('}', lb1 + 1);
  auto lb2 = in.find('{', rb1 + 1); auto rb2 = in.find('}', lb2 + 1);
  if (lb1 == std::string::npos || rb1 == std::string::npos || lb2 == std::string::npos || rb2 == std::string::npos) {
    throw std::runtime_error("invalid \\frac syntax");
  }
  P np(in.substr(lb1 + 1, rb1 - lb1 - 1));
  P dp(in.substr(lb2 + 1, rb2 - lb2 - 1));
  return std::make_unique<Fraction>(np.expr(), dp.expr());
}

}  // namespace

std::unique_ptr<Expr> parse(std::string input) {
  if (input.rfind("\\frac", 0) == 0) return parse_frac(input);
  P p(std::move(input));
  auto out = p.expr();
  p.ws();
  if (p.i != p.s.size()) throw std::runtime_error("unexpected trailing input");
  return out;
}

}  // namespace overcalc
