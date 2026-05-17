#include "overcalc/latex.hpp"

#include "overcalc/symbols.hpp"

#include <stdexcept>

namespace overcalc {
namespace {

int precedence(const Expr& e) {
  if (auto b = dynamic_cast<const Binary*>(&e)) return (b->op == '+' || b->op == '-') ? 1 : 2;
  return dynamic_cast<const Superscript*>(&e) ? 3 : 4;
}

std::string latex_inner(const Expr& expr, int parent_prec, bool right_child, char parent_op);

std::string maybe_paren(const Expr& e, const std::string& out, int parent_prec, bool right_child, char parent_op) {
  int child_prec = precedence(e);
  bool need = child_prec < parent_prec;
  need = need || (right_child && child_prec == parent_prec && (parent_op == '-' || parent_op == '/'));
  return need ? "(" + out + ")" : out;
}

std::string latex_inner(const Expr& expr, int parent_prec, bool right_child, char parent_op) {
  std::string out;
  if (auto n = dynamic_cast<const Number*>(&expr)) out = std::to_string(n->value);
  else if (auto d = dynamic_cast<const Decimal*>(&expr)) out = d->text;
  else if (auto s = dynamic_cast<const Symbol*>(&expr)) {
    if (auto command = symbol_to_latex_command(s->name)) out = "\\" + *command;
    else out = s->name;
  } else if (auto fn = dynamic_cast<const FunctionCall*>(&expr)) {
    if(fn->name=="fact") out = latex_inner(*fn->arg, 4, false, '!') + "!";
    else if(fn->name=="percent") out = latex_inner(*fn->arg, 4, false, '%') + "\\%";
    else out = "\\" + fn->name + "{" + latex_inner(*fn->arg, 0, false, 0) + "}";
  }
  else if (auto f = dynamic_cast<const Fraction*>(&expr)) out = "\\frac{" + latex_inner(*f->num, 0, false, 0) + "}{" + latex_inner(*f->den, 0, false, 0) + "}";
  else if (auto q = dynamic_cast<const Sqrt*>(&expr)) out = q->index ? "\\sqrt[" + latex_inner(*q->index, 0, false, 0) + "]{" + latex_inner(*q->radicand, 0, false, 0) + "}" : "\\sqrt{" + latex_inner(*q->radicand, 0, false, 0) + "}";
  else if (auto p = dynamic_cast<const Superscript*>(&expr)) out = maybe_paren(*p->base, latex_inner(*p->base, 0, false, 0), 3, false, '^') + "^{" + latex_inner(*p->exponent, 0, false, 0) + "}";
  else if (auto sb = dynamic_cast<const Subscript*>(&expr)) out = maybe_paren(*sb->base, latex_inner(*sb->base, 0, false, 0), 3, false, '_') + "_{" + latex_inner(*sb->sub, 0, false, 0) + "}";
  else if (auto b = dynamic_cast<const Binary*>(&expr)) {
    if(b->op=='*'){
      if(auto n=dynamic_cast<const Number*>(b->lhs.get()); n&&n->value==-1){
        out = "-" + latex_inner(*b->rhs, 3, false, '-');
        return maybe_paren(expr, out, parent_prec, right_child, parent_op);
      }
    }
    int prec = precedence(expr);
    std::string op = b->op == '*' ? "\\cdot" : std::string(1, b->op);
    out = latex_inner(*b->lhs, prec, false, b->op) + op + latex_inner(*b->rhs, prec, true, b->op);
  } else {
    throw std::runtime_error("unknown node in latex serialization");
  }
  return maybe_paren(expr, out, parent_prec, right_child, parent_op);
}

}  // namespace

std::string to_latex(const Expr& expr) {
  return latex_inner(expr, 0, false, 0);
}

}  // namespace overcalc
