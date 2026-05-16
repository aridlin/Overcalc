#include "overcalc/eval.hpp"

#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
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

std::string rational_to_string(R r) {
  r = simp(r);
  if (r.d == 1) return std::to_string(r.n);
  return std::to_string(r.n) + "/" + std::to_string(r.d);
}

std::string paren_if_bin(const Expr& e, const std::string& s) {
  return dynamic_cast<const Binary*>(&e) ? "(" + s + ")" : s;
}

std::string exact_of(const Expr& e) {
  if (auto n = dynamic_cast<const Number*>(&e)) return std::to_string(n->value);
  if (auto s = dynamic_cast<const Symbol*>(&e)) return s->name;
  if (auto f = dynamic_cast<const Fraction*>(&e)) return "(" + exact_of(*f->num) + ")/" + "(" + exact_of(*f->den) + ")";
  if (auto q = dynamic_cast<const Sqrt*>(&e)) return "sqrt(" + exact_of(*q->radicand) + ")";
  if (auto p = dynamic_cast<const Superscript*>(&e)) return paren_if_bin(*p->base, exact_of(*p->base)) + "^" + paren_if_bin(*p->exponent, exact_of(*p->exponent));
  if (auto sb = dynamic_cast<const Subscript*>(&e)) return exact_of(*sb->base) + "_" + exact_of(*sb->sub);
  auto b = dynamic_cast<const Binary*>(&e);
  if (!b) throw std::runtime_error("unknown node");
  return paren_if_bin(*b->lhs, exact_of(*b->lhs)) + " " + std::string(1, b->op) + " " + paren_if_bin(*b->rhs, exact_of(*b->rhs));
}

std::optional<R> try_exact_rational(const Expr& e) {
  if (auto n = dynamic_cast<const Number*>(&e)) return R{n->value, 1};
  if (dynamic_cast<const Symbol*>(&e)) return std::nullopt;
  if (auto f = dynamic_cast<const Fraction*>(&e)) {
    auto a = try_exact_rational(*f->num), b = try_exact_rational(*f->den);
    if (!a || !b) return std::nullopt;
    return simp({a->n * b->d, a->d * b->n});
  }
  if (auto q = dynamic_cast<const Sqrt*>(&e)) {
    auto r = try_exact_rational(*q->radicand);
    if (!r || r->d != 1 || r->n < 0) return std::nullopt;
    auto root = static_cast<long long>(std::llround(std::sqrt((long double)r->n)));
    if (root * root != r->n) return std::nullopt;
    return R{root, 1};
  }
  if (auto p = dynamic_cast<const Superscript*>(&e)) {
    auto b = try_exact_rational(*p->base), ex = try_exact_rational(*p->exponent);
    if (!b || !ex || ex->d != 1 || ex->n < 0) return std::nullopt;
    R out{1, 1};
    for (long long i = 0; i < ex->n; ++i) out = simp({out.n * b->n, out.d * b->d});
    return out;
  }
  if (auto sb = dynamic_cast<const Subscript*>(&e)) return try_exact_rational(*sb->base);
  auto b = dynamic_cast<const Binary*>(&e);
  if (!b) return std::nullopt;
  auto l = try_exact_rational(*b->lhs), r = try_exact_rational(*b->rhs);
  if (!l || !r) return std::nullopt;
  switch (b->op) {
    case '+': return simp({l->n * r->d + r->n * l->d, l->d * r->d});
    case '-': return simp({l->n * r->d - r->n * l->d, l->d * r->d});
    case '*': return simp({l->n * r->n, l->d * r->d});
    case '/': return simp({l->n * r->d, l->d * r->n});
    default: return std::nullopt;
  }
}

std::optional<long double> approx_of(const Expr& e) {
  if (auto n = dynamic_cast<const Number*>(&e)) return static_cast<long double>(n->value);
  if (dynamic_cast<const Symbol*>(&e)) return std::nullopt;
  if (auto f = dynamic_cast<const Fraction*>(&e)) {
    auto a = approx_of(*f->num), b = approx_of(*f->den); if (!a || !b) return std::nullopt; return *a / *b;
  }
  if (auto q = dynamic_cast<const Sqrt*>(&e)) { auto r = approx_of(*q->radicand); if (!r || *r < 0) return std::nullopt; return std::sqrt(*r); }
  if (auto p = dynamic_cast<const Superscript*>(&e)) { auto b = approx_of(*p->base), ex = approx_of(*p->exponent); if (!b || !ex) return std::nullopt; return std::pow(*b, *ex); }
  if (auto sb = dynamic_cast<const Subscript*>(&e)) return approx_of(*sb->base);
  auto b = dynamic_cast<const Binary*>(&e);
  if (!b) return std::nullopt;
  auto l = approx_of(*b->lhs), r = approx_of(*b->rhs); if (!l || !r) return std::nullopt;
  switch (b->op) { case '+': return *l + *r; case '-': return *l - *r; case '*': return *l * *r; case '/': return *l / *r; default: return std::nullopt; }
}

}  // namespace

EvalResult evaluate(const Expr& expr) {
  EvalResult out;
  if (auto exact_r = try_exact_rational(expr)) out.exact = rational_to_string(*exact_r);
  else out.exact = exact_of(expr);
  out.decimal = approx_of(expr);
  return out;
}

std::string evaluate_to_string(const Expr& expr) {
  return evaluate(expr).exact;
}

}  // namespace overcalc
