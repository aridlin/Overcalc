#include "overcalc/eval.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace overcalc {
namespace {

struct R { long long n; long long d; };

R simp(R r) {
  if (r.d == 0) throw std::runtime_error("division by zero");
  if (r.d < 0) { r.n = -r.n; r.d = -r.d; }
  auto g = std::gcd(r.n < 0 ? -r.n : r.n, r.d);
  if (g) { r.n /= g; r.d /= g; }
  return r;
}

bool is_int(const R& r) { return r.d == 1; }

R ipow(R base, long long exp) {
  if (exp < 0) throw std::runtime_error("negative exponents not supported in numeric evaluator");
  R out{1,1};
  for (long long i = 0; i < exp; ++i) out = simp({out.n * base.n, out.d * base.d});
  return out;
}

R ev(const Expr& e) {
  if (auto n = dynamic_cast<const Number*>(&e)) return {n->value, 1};
  if (auto s = dynamic_cast<const Symbol*>(&e)) throw std::runtime_error("cannot numerically evaluate symbol: " + s->name);
  if (auto f = dynamic_cast<const Fraction*>(&e)) {
    auto a = ev(*f->num), b = ev(*f->den);
    return simp({a.n * b.d, a.d * b.n});
  }
  if (auto q = dynamic_cast<const Sqrt*>(&e)) {
    auto r = ev(*q->radicand);
    if (!is_int(r)) throw std::runtime_error("sqrt of non-integer rational is not supported yet");
    auto root = static_cast<long long>(std::llround(std::sqrt(static_cast<long double>(r.n))));
    if (root * root != r.n) throw std::runtime_error("irrational sqrt in numeric exact mode");
    return {root, 1};
  }
  if (auto p = dynamic_cast<const Superscript*>(&e)) {
    auto b = ev(*p->base), ex = ev(*p->exponent);
    if (!is_int(ex)) throw std::runtime_error("fractional exponent not supported");
    return ipow(b, ex.n);
  }
  if (auto sb = dynamic_cast<const Subscript*>(&e)) return ev(*sb->base);

  auto b = dynamic_cast<const Binary*>(&e);
  if (!b) throw std::runtime_error("unknown node");
  auto l = ev(*b->lhs), r = ev(*b->rhs);
  switch (b->op) {
    case '+': return simp({l.n * r.d + r.n * l.d, l.d * r.d});
    case '-': return simp({l.n * r.d - r.n * l.d, l.d * r.d});
    case '*': return simp({l.n * r.n, l.d * r.d});
    case '/': return simp({l.n * r.d, l.d * r.n});
    default: throw std::runtime_error("unsupported operator");
  }
}

}  // namespace

std::string evaluate_to_string(const Expr& expr) {
  auto r = simp(ev(expr));
  if (r.d == 1) return std::to_string(r.n);
  return std::to_string(r.n) + "/" + std::to_string(r.d);
}

}  // namespace overcalc
