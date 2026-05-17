#include "overcalc/parser.hpp"

#include <cctype>
#include <stdexcept>

namespace overcalc {
namespace {

struct P {
  std::string s; std::size_t i = 0;
  explicit P(std::string in) : s(std::move(in)) {}
  void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
  bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }
  bool starts_atom() { ws(); if (i>=s.size()) return false; char c=s[i]; return c=='(' || c=='\\' || std::isdigit((unsigned char)c) || std::isalpha((unsigned char)c); }

  std::string brace_content() {
    ws(); if (i >= s.size() || s[i] != '{') throw std::runtime_error("expected {");
    ++i; int depth=1; std::size_t start=i;
    while (i < s.size() && depth>0) { if (s[i]=='{') ++depth; else if (s[i]=='}') --depth; ++i; }
    if (depth!=0) throw std::runtime_error("unclosed brace");
    return s.substr(start, (i-1)-start);
  }

  std::unique_ptr<Expr> number(){ ws(); if(i>=s.size()||!std::isdigit((unsigned char)s[i])) throw std::runtime_error("expected number"); long long v=0; while(i<s.size()&&std::isdigit((unsigned char)s[i])) v=v*10+(s[i++]-'0'); return std::make_unique<Number>(v);} 
  std::unique_ptr<Expr> symbol(){ ws(); if(i>=s.size()||!std::isalpha((unsigned char)s[i])) throw std::runtime_error("expected symbol"); std::string n; while(i<s.size()&&(std::isalnum((unsigned char)s[i])||s[i]=='_')) n.push_back(s[i++]); return std::make_unique<Symbol>(n);} 
  std::unique_ptr<Expr> latex_frac(){ i+=5; P np(brace_content()); P dp(brace_content()); return std::make_unique<Fraction>(np.expr(),dp.expr()); }
  std::unique_ptr<Expr> latex_sqrt(){ i+=5; P rp(brace_content()); return std::make_unique<Sqrt>(rp.expr()); }

  std::unique_ptr<Expr> atom(){
    ws();
    if (eat('-')) return std::make_unique<Binary>('*', std::make_unique<Number>(-1), atom());
    if (eat('(')) { auto e=expr(); if(!eat(')')) throw std::runtime_error("missing )"); return e; }
    if (i<s.size() && s[i]=='\\') { if(s.compare(i,5,"\\frac")==0) return latex_frac(); if(s.compare(i,5,"\\sqrt")==0) return latex_sqrt(); throw std::runtime_error("unknown latex command"); }
    if (i<s.size() && std::isdigit((unsigned char)s[i])) return number();
    return symbol();
  }

  std::unique_ptr<Expr> postfix(){ auto base=atom(); while(true){ ws(); if(eat('^')){ std::unique_ptr<Expr> ex; ws(); if(i<s.size()&&s[i]=='{'){ P sp(brace_content()); ex=sp.expr(); } else ex=atom(); base=std::make_unique<Superscript>(std::move(base),std::move(ex)); }
      else if(eat('_')){ std::unique_ptr<Expr> sub; ws(); if(i<s.size()&&s[i]=='{'){ P sp(brace_content()); sub=sp.expr(); } else sub=atom(); base=std::make_unique<Subscript>(std::move(base),std::move(sub)); }
      else break; } return base; }

  std::unique_ptr<Expr> term(){ auto l=postfix(); while(true){ ws(); if(i<s.size()&&(s[i]=='*'||s[i]=='/')){ char op=s[i++]; auto r=postfix(); l=std::make_unique<Binary>(op,std::move(l),std::move(r)); }
      else if (starts_atom()) { auto r=postfix(); l=std::make_unique<Binary>('*', std::move(l), std::move(r)); }
      else break; } return l; }

  std::unique_ptr<Expr> expr(){ auto l=term(); while(true){ ws(); if(i>=s.size()||(s[i]!='+'&&s[i]!='-')) break; char op=s[i++]; auto r=term(); l=std::make_unique<Binary>(op,std::move(l),std::move(r)); } return l; }
};

}  // namespace

std::unique_ptr<Expr> parse(std::string input){ P p(std::move(input)); auto out=p.expr(); p.ws(); if(p.i!=p.s.size()) throw std::runtime_error("unexpected trailing input"); return out; }
} // namespace overcalc
