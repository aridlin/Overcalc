#include "overcalc/ast_dump.hpp"
#include <sstream>
#include <stdexcept>

namespace overcalc {
std::string ast_to_json(const Expr& expr) {
  std::ostringstream o;
  if (auto n = dynamic_cast<const Number*>(&expr)) { o << "{\"type\":\"Number\",\"value\":" << n->value << "}"; return o.str(); }
  if (auto s = dynamic_cast<const Symbol*>(&expr)) { o << "{\"type\":\"Symbol\",\"name\":\"" << s->name << "\"}"; return o.str(); }
  if (auto f = dynamic_cast<const Fraction*>(&expr)) { o << "{\"type\":\"Fraction\",\"num\":" << ast_to_json(*f->num) << ",\"den\":" << ast_to_json(*f->den) << "}"; return o.str(); }
  if (auto q = dynamic_cast<const Sqrt*>(&expr)) { o << "{\"type\":\"Sqrt\",\"radicand\":" << ast_to_json(*q->radicand) << "}"; return o.str(); }
  if (auto p = dynamic_cast<const Superscript*>(&expr)) { o << "{\"type\":\"Superscript\",\"base\":" << ast_to_json(*p->base) << ",\"exp\":" << ast_to_json(*p->exponent) << "}"; return o.str(); }
  if (auto sb = dynamic_cast<const Subscript*>(&expr)) { o << "{\"type\":\"Subscript\",\"base\":" << ast_to_json(*sb->base) << ",\"sub\":" << ast_to_json(*sb->sub) << "}"; return o.str(); }
  if (auto b = dynamic_cast<const Binary*>(&expr)) { o << "{\"type\":\"Binary\",\"op\":\"" << b->op << "\",\"lhs\":" << ast_to_json(*b->lhs) << ",\"rhs\":" << ast_to_json(*b->rhs) << "}"; return o.str(); }
  throw std::runtime_error("unknown node in ast json");
}
}
