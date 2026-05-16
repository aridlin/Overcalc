#include "overcalc/eval.hpp"
#include <numeric>
#include <stdexcept>
namespace overcalc { namespace { struct R{ long long n; long long d;};
R simp(R r){ if(r.d==0) throw std::runtime_error("division by zero"); if(r.d<0){r.n=-r.n;r.d=-r.d;} auto g=std::gcd(r.n<0?-r.n:r.n,r.d); if(g){r.n/=g;r.d/=g;} return r; }
R ev(const Expr& e){ if(auto n=dynamic_cast<const Number*>(&e)) return {n->value,1}; if(auto f=dynamic_cast<const Fraction*>(&e)){ auto a=ev(*f->num), b=ev(*f->den); return simp({a.n*b.d,a.d*b.n}); } auto b=dynamic_cast<const Binary*>(&e); if(!b) throw std::runtime_error("unknown node"); auto l=ev(*b->lhs), r=ev(*b->rhs); switch(b->op){ case '+': return simp({l.n*r.d+r.n*l.d,l.d*r.d}); case '-': return simp({l.n*r.d-r.n*l.d,l.d*r.d}); case '*': return simp({l.n*r.n,l.d*r.d}); case '/': return simp({l.n*r.d,l.d*r.n}); default: throw std::runtime_error("unsupported operator"); }} }
std::string evaluate_to_string(const Expr& expr){ auto r=simp(ev(expr)); if(r.d==1) return std::to_string(r.n); return std::to_string(r.n)+"/"+std::to_string(r.d);} }
