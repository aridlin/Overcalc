#include "overcalc/eval.hpp"

#include "overcalc/format.hpp"

#include <cmath>
#include <numeric>
#include <sstream>

namespace overcalc {
namespace {
struct R { long long n; long long d; };
R simp(R r){ if(r.d==0) throw std::runtime_error("division by zero"); if(r.d<0){r.n=-r.n;r.d=-r.d;} auto g=std::gcd(r.n<0?-r.n:r.n,r.d); if(g){r.n/=g;r.d/=g;} return r; }
std::string rstr(R r){ r=simp(r); if(r.d==1) return std::to_string(r.n); return std::to_string(r.n)+"/"+std::to_string(r.d);} 
long long ipow(long long base, long long exp){ long long out=1; for(long long i=0;i<exp;++i) out*=base; return out; }
std::optional<long long> factorial(long long value){
  if(value<0||value>20) return std::nullopt;
  long long out=1; for(long long i=2;i<=value;++i) out*=i; return out;
}

std::optional<long long> exact_int_root(long long value, long long index) {
  if(index<=0) return std::nullopt;
  if(value<0 && index%2==0) return std::nullopt;
  long double mag = std::llabs(value);
  long long root = llround(powl(mag, 1.0L / (long double)index));
  if(ipow(root,index)!=mag) return std::nullopt;
  return value<0 ? -root : root;
}

std::optional<R> exact(const Expr& e, StepLog* s){
  if(auto n=dynamic_cast<const Number*>(&e)) return R{n->value,1};
  if(dynamic_cast<const Decimal*>(&e)) return std::nullopt;
  if(auto sym=dynamic_cast<const Symbol*>(&e)){ if(sym->name=="pi"||sym->name=="e") return std::nullopt; return std::nullopt; }
  if(auto fn=dynamic_cast<const FunctionCall*>(&e)){
    if(fn->name=="abs"){ auto v=exact(*fn->arg,s); if(!v) return std::nullopt; if(v->n<0) v->n=-v->n; return *v; }
    if(fn->name=="fact"){ auto v=exact(*fn->arg,s); if(!v||v->d!=1) return std::nullopt; auto f=factorial(v->n); if(!f) return std::nullopt; return R{*f,1}; }
    if(fn->name=="percent"){ auto v=exact(*fn->arg,s); if(!v) return std::nullopt; return simp({v->n,v->d*100}); }
    if(auto v=exact(*fn->arg,s)){
      if(v->n==0 && v->d==1 && (fn->name=="sin"||fn->name=="tan"||fn->name=="ln")) return R{0,1};
      if(v->n==0 && v->d==1 && (fn->name=="cos"||fn->name=="exp")) return R{1,1};
      if(v->n==1 && v->d==1 && fn->name=="ln") return R{0,1};
    }
    if(fn->name=="ln"){ if(auto sym=dynamic_cast<const Symbol*>(fn->arg.get()); sym&&sym->name=="e") return R{1,1}; }
    if(fn->name=="sin"){
      if(auto f=dynamic_cast<const Binary*>(fn->arg.get()); f&&f->op=='/'){
        if(auto sym=dynamic_cast<const Symbol*>(f->lhs.get()); sym&&sym->name=="pi"){
          if(auto n=dynamic_cast<const Number*>(f->rhs.get()); n&&n->value==2) return R{1,1};
        }
      }
      if(auto sym=dynamic_cast<const Symbol*>(fn->arg.get()); sym&&sym->name=="pi") return R{0,1};
    }
    if(fn->name=="cos"){
      if(auto f=dynamic_cast<const Binary*>(fn->arg.get()); f&&f->op=='/'){
        if(auto sym=dynamic_cast<const Symbol*>(f->lhs.get()); sym&&sym->name=="pi"){
          if(auto n=dynamic_cast<const Number*>(f->rhs.get()); n&&n->value==2) return R{0,1};
        }
      }
      if(auto sym=dynamic_cast<const Symbol*>(fn->arg.get()); sym&&sym->name=="pi") return R{-1,1};
    }
    return std::nullopt;
  }
  if(auto f=dynamic_cast<const Fraction*>(&e)){ auto a=exact(*f->num,s), b=exact(*f->den,s); if(!a||!b) return std::nullopt; return simp({a->n*b->d,a->d*b->n}); }
  if(auto q=dynamic_cast<const Sqrt*>(&e)){
    auto v=exact(*q->radicand,s); if(!v) return std::nullopt;
    long long idx=2;
    if(q->index){ auto i=exact(*q->index,s); if(!i||i->d!=1) return std::nullopt; idx=i->n; }
    auto rn=exact_int_root(v->n,idx), rd=exact_int_root(v->d,idx);
    if(!rn||!rd) return std::nullopt;
    return simp({*rn,*rd});
  }
  if(auto p=dynamic_cast<const Superscript*>(&e)){ auto b=exact(*p->base,s), ex=exact(*p->exponent,s); if(!b||!ex||ex->d!=1||ex->n<0) return std::nullopt; R out{1,1}; for(long long i=0;i<ex->n;++i) out=simp({out.n*b->n,out.d*b->d}); return out; }
  if(auto sb=dynamic_cast<const Subscript*>(&e)) return exact(*sb->base,s);
  auto b=dynamic_cast<const Binary*>(&e); if(!b) return std::nullopt; auto l=exact(*b->lhs,s), r=exact(*b->rhs,s); if(!l||!r) return std::nullopt; 
  switch(b->op){ case '+': return simp({l->n*r->d+r->n*l->d,l->d*r->d}); case '-': return simp({l->n*r->d-r->n*l->d,l->d*r->d}); case '*': return simp({l->n*r->n,l->d*r->d}); case '/': return simp({l->n*r->d,l->d*r->n}); default: return std::nullopt; }
}

std::string symbolic(const Expr& e){
  if(auto n=dynamic_cast<const Number*>(&e)) return std::to_string(n->value);
  if(auto d=dynamic_cast<const Decimal*>(&e)) return d->text;
  if(auto sym=dynamic_cast<const Symbol*>(&e)) return sym->name;
  if(auto fn=dynamic_cast<const FunctionCall*>(&e)) return fn->name+"("+symbolic(*fn->arg)+")";
  if(auto f=dynamic_cast<const Fraction*>(&e)) return "("+symbolic(*f->num)+")/("+symbolic(*f->den)+")";
  if(auto q=dynamic_cast<const Sqrt*>(&e)) return q->index ? "root("+symbolic(*q->index)+","+symbolic(*q->radicand)+")" : "sqrt("+symbolic(*q->radicand)+")";
  if(auto p=dynamic_cast<const Superscript*>(&e)) return symbolic(*p->base)+"^"+symbolic(*p->exponent);
  if(auto sb=dynamic_cast<const Subscript*>(&e)) return symbolic(*sb->base)+"_"+symbolic(*sb->sub);
  auto b=dynamic_cast<const Binary*>(&e); if(!b) throw std::runtime_error("unknown node"); return "("+symbolic(*b->lhs)+")"+b->op+"("+symbolic(*b->rhs)+")";
}

std::optional<long double> approx(const Expr& e){
  if(auto n=dynamic_cast<const Number*>(&e)) return (long double)n->value;
  if(auto d=dynamic_cast<const Decimal*>(&e)) return d->value;
  if(auto sym=dynamic_cast<const Symbol*>(&e)){ if(sym->name=="pi") return acosl(-1.0L); if(sym->name=="e") return expl(1.0L); return std::nullopt; }
  if(auto fn=dynamic_cast<const FunctionCall*>(&e)){
    auto v=approx(*fn->arg); if(!v) return std::nullopt;
    if(fn->name=="sin") return sinl(*v);
    if(fn->name=="cos") return cosl(*v);
    if(fn->name=="tan") return tanl(*v);
    if(fn->name=="ln"||fn->name=="log") return *v>0 ? std::optional<long double>{logl(*v)} : std::nullopt;
    if(fn->name=="exp") return expl(*v);
    if(fn->name=="abs") return fabsl(*v);
    if(fn->name=="fact"){ if(*v<0) return std::nullopt; return tgammal(*v+1); }
    if(fn->name=="percent") return *v/100.0L;
    return std::nullopt;
  }
  if(auto f=dynamic_cast<const Fraction*>(&e)){ auto a=approx(*f->num), b=approx(*f->den); if(!a||!b) return std::nullopt; return *a / *b; }
  if(auto q=dynamic_cast<const Sqrt*>(&e)){
    auto v=approx(*q->radicand); if(!v) return std::nullopt;
    long double idx=2;
    if(q->index){ auto i=approx(*q->index); if(!i||*i==0) return std::nullopt; idx=*i; }
    if(*v<0 && std::fmod(idx,2.0L)!=1.0L) return std::nullopt;
    return *v<0 ? -powl(-*v,1.0L/idx) : powl(*v,1.0L/idx);
  } 
  if(auto p=dynamic_cast<const Superscript*>(&e)){ auto b=approx(*p->base), ex=approx(*p->exponent); if(!b||!ex) return std::nullopt; return powl(*b,*ex);} 
  if(auto sb=dynamic_cast<const Subscript*>(&e)) return approx(*sb->base);
  auto b=dynamic_cast<const Binary*>(&e); if(!b) return std::nullopt; auto l=approx(*b->lhs), r=approx(*b->rhs); if(!l||!r) return std::nullopt; switch(b->op){ case '+': return *l+*r; case '-': return *l-*r; case '*': return *l**r; case '/': return *l/ *r; default: return std::nullopt; }
}
}

EvalResult evaluate(const Expr& expr, StepLog* steps){
  if(steps) steps->add("Start evaluation");
  EvalResult out; auto ex=exact(expr,steps); if(ex){ out.exact=rstr(*ex); if(steps) steps->add("Exact rational: "+out.exact);} else { out.exact=to_infix(expr); if(steps) steps->add("Symbolic exact form: "+out.exact);} out.decimal=approx(expr); if(out.decimal&&steps){ std::ostringstream o; o<<"Approx decimal: "<<(double)*out.decimal; steps->add(o.str()); } return out;
}

std::string evaluate_to_string(const Expr& expr){ return evaluate(expr).exact; }
} // namespace overcalc
