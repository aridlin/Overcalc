#include "overcalc/format.hpp"
#include "overcalc/symbols.hpp"
#include <stdexcept>

namespace overcalc {
namespace {

int precedence(const Expr& e) {
  if (auto b = dynamic_cast<const Binary*>(&e)) return (b->op == '+' || b->op == '-') ? 1 : 2;
  return dynamic_cast<const Superscript*>(&e) ? 3 : 4;
}

std::string format_infix(const Expr& expr, int parent_prec, bool right_child, char parent_op);

std::string maybe_paren(const Expr& e, const std::string& s, int parent_prec, bool right_child, char parent_op) {
  int child_prec = precedence(e);
  bool need = child_prec < parent_prec;
  need = need || (right_child && child_prec == parent_prec && (parent_op == '-' || parent_op == '/'));
  return need ? "(" + s + ")" : s;
}

std::string format_infix(const Expr& expr, int parent_prec, bool right_child, char parent_op) {
  std::string out;
  if (auto n = dynamic_cast<const Number*>(&expr)) out = std::to_string(n->value);
  else if (auto d = dynamic_cast<const Decimal*>(&expr)) out = d->text;
  else if (auto s = dynamic_cast<const Symbol*>(&expr)) out = render_symbol(s->name, true);
  else if (auto fn = dynamic_cast<const FunctionCall*>(&expr)) {
    if(fn->name=="fact") out = format_infix(*fn->arg, 4, false, '!') + "!";
    else if(fn->name=="percent") out = format_infix(*fn->arg, 4, false, '%') + "%";
    else out = fn->name + "(" + format_infix(*fn->arg, 0, false, 0) + ")";
  }
  else if (auto f = dynamic_cast<const Fraction*>(&expr)) out = "(" + format_infix(*f->num, 0, false, 0) + ")/(" + format_infix(*f->den, 0, false, 0) + ")";
  else if (auto q = dynamic_cast<const Sqrt*>(&expr)) out = q->index ? "root[" + format_infix(*q->index, 0, false, 0) + "](" + format_infix(*q->radicand, 0, false, 0) + ")" : "sqrt(" + format_infix(*q->radicand, 0, false, 0) + ")";
  else if (auto p = dynamic_cast<const Superscript*>(&expr)) out = maybe_paren(*p->base, format_infix(*p->base, 0, false, 0), 3, false, '^') + "^" + maybe_paren(*p->exponent, format_infix(*p->exponent, 0, false, 0), 3, true, '^');
  else if (auto sb = dynamic_cast<const Subscript*>(&expr)) out = format_infix(*sb->base, 4, false, '_') + "_" + format_infix(*sb->sub, 4, true, '_');
  else if (auto b = dynamic_cast<const Binary*>(&expr)) {
    if(b->op=='*'){
      if(auto n=dynamic_cast<const Number*>(b->lhs.get()); n&&n->value==-1){
        out = "-" + format_infix(*b->rhs, 3, false, '-');
        return maybe_paren(expr, out, parent_prec, right_child, parent_op);
      }
    }
    int prec = precedence(expr);
    out = format_infix(*b->lhs, prec, false, b->op) + " " + std::string(1,b->op) + " " + format_infix(*b->rhs, prec, true, b->op);
  } else {
    throw std::runtime_error("unknown node in infix");
  }
  return maybe_paren(expr, out, parent_prec, right_child, parent_op);
}

}

std::string to_infix(const Expr& expr) {
  return format_infix(expr, 0, false, 0);
}
}
