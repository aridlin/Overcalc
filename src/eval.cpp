#include "overcalc/eval.hpp"

#include <cmath>
#include <numeric>
#include <sstream>

namespace overcalc {
namespace {
struct R { long long n; long long d; };
R simp(R r){ if(r.d==0) throw std::runtime_error("division by zero"); if(r.d<0){r.n=-r.n;r.d=-r.d;} auto g=std::gcd(r.n<0?-r.n:r.n,r.d); if(g){r.n/=g;r.d/=g;} return r; }
std::string rstr(R r){ r=simp(r); if(r.d==1) return std::to_string(r.n); return std::to_string(r.n)+"/"+std::to_string(r.d);} 

std::optional<R> exact(const Expr& e, StepLog* s){
  if(auto n=dynamic_cast<const Number*>(&e)) return R{n->value,1};
  if(auto sym=dynamic_cast<const Symbol*>(&e)){ if(sym->name=="pi"||sym->name=="e") return std::nullopt; return std::nullopt; }
  if(auto f=dynamic_cast<const Fraction*>(&e)){ auto a=exact(*f->num,s), b=exact(*f->den,s); if(!a||!b) return std::nullopt; return simp({a->n*b->d,a->d*b->n}); }
  if(auto q=dynamic_cast<const Sqrt*>(&e)){ auto v=exact(*q->radicand,s); if(!v||v->d!=1||v->n<0) return std::nullopt; long long rt=llround(std::sqrt((long double)v->n)); if(rt*rt!=v->n) return std::nullopt; return R{rt,1}; }
  if(auto p=dynamic_cast<const Superscript*>(&e)){ auto b=exact(*p->base,s), ex=exact(*p->exponent,s); if(!b||!ex||ex->d!=1||ex->n<0) return std::nullopt; R out{1,1}; for(long long i=0;i<ex->n;++i) out=simp({out.n*b->n,out.d*b->d}); return out; }
  if(auto sb=dynamic_cast<const Subscript*>(&e)) return exact(*sb->base,s);
  auto b=dynamic_cast<const Binary*>(&e); if(!b) return std::nullopt; auto l=exact(*b->lhs,s), r=exact(*b->rhs,s); if(!l||!r) return std::nullopt; 
  switch(b->op){ case '+': return simp({l->n*r->d+r->n*l->d,l->d*r->d}); case '-': return simp({l->n*r->d-r->n*l->d,l->d*r->d}); case '*': return simp({l->n*r->n,l->d*r->d}); case '/': return simp({l->n*r->d,l->d*r->n}); default: return std::nullopt; }
}

std::string symbolic(const Expr& e){
  if(auto n=dynamic_cast<const Number*>(&e)) return std::to_string(n->value);
  if(auto sym=dynamic_cast<const Symbol*>(&e)) return sym->name;
  if(auto f=dynamic_cast<const Fraction*>(&e)) return "("+symbolic(*f->num)+")/("+symbolic(*f->den)+")";
  if(auto q=dynamic_cast<const Sqrt*>(&e)) return "sqrt("+symbolic(*q->radicand)+")";
  if(auto p=dynamic_cast<const Superscript*>(&e)) return symbolic(*p->base)+"^"+symbolic(*p->exponent);
  if(auto sb=dynamic_cast<const Subscript*>(&e)) return symbolic(*sb->base)+"_"+symbolic(*sb->sub);
  auto b=dynamic_cast<const Binary*>(&e); if(!b) throw std::runtime_error("unknown node"); return "("+symbolic(*b->lhs)+")"+b->op+"("+symbolic(*b->rhs)+")";
}

std::optional<long double> approx(const Expr& e){
  if(auto n=dynamic_cast<const Number*>(&e)) return (long double)n->value;
  if(auto sym=dynamic_cast<const Symbol*>(&e)){ if(sym->name=="pi") return acosl(-1.0L); if(sym->name=="e") return expl(1.0L); return std::nullopt; }
  if(auto f=dynamic_cast<const Fraction*>(&e)){ auto a=approx(*f->num), b=approx(*f->den); if(!a||!b) return std::nullopt; return *a / *b; }
  if(auto q=dynamic_cast<const Sqrt*>(&e)){ auto v=approx(*q->radicand); if(!v||*v<0) return std::nullopt; return sqrtl(*v);} 
  if(auto p=dynamic_cast<const Superscript*>(&e)){ auto b=approx(*p->base), ex=approx(*p->exponent); if(!b||!ex) return std::nullopt; return powl(*b,*ex);} 
  if(auto sb=dynamic_cast<const Subscript*>(&e)) return approx(*sb->base);
  auto b=dynamic_cast<const Binary*>(&e); if(!b) return std::nullopt; auto l=approx(*b->lhs), r=approx(*b->rhs); if(!l||!r) return std::nullopt; switch(b->op){ case '+': return *l+*r; case '-': return *l-*r; case '*': return *l**r; case '/': return *l/ *r; default: return std::nullopt; }
}
}

EvalResult evaluate(const Expr& expr, StepLog* steps){
  if(steps) steps->add("Start evaluation");
  EvalResult out; auto ex=exact(expr,steps); if(ex){ out.exact=rstr(*ex); if(steps) steps->add("Exact rational: "+out.exact);} else { out.exact=symbolic(expr); if(steps) steps->add("Symbolic exact form: "+out.exact);} out.decimal=approx(expr); if(out.decimal&&steps){ std::ostringstream o; o<<"Approx decimal: "<<(double)*out.decimal; steps->add(o.str()); } return out;
}

std::string evaluate_to_string(const Expr& expr){ return evaluate(expr).exact; }
} // namespace overcalc
