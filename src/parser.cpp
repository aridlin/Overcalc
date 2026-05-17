#include "overcalc/parser.hpp"

#include "overcalc/diagnostics.hpp"
#include "overcalc/symbols.hpp"

#include <cctype>
#include <stdexcept>

namespace overcalc {
namespace {

struct P {
  std::string s; std::size_t i = 0;
  explicit P(std::string in) : s(std::move(in)) {}
  bool starts_latex_command(const std::string& name) const {
    if (i >= s.size() || s[i] != '\\') return false;
    std::size_t p = i + 1;
    for (char c : name) {
      if (p >= s.size() || s[p] != c) return false;
      ++p;
    }
    return p >= s.size() || !std::isalpha((unsigned char)s[p]);
  }
  bool eat_latex_space() {
    if (i >= s.size() || s[i] != '\\') return false;
    std::size_t start = i++;
    if (i < s.size() && (s[i] == ',' || s[i] == ';' || s[i] == ':' || s[i] == '!' || s[i] == ' ')) {
      ++i;
      return true;
    }
    std::string command;
    while (i < s.size() && std::isalpha((unsigned char)s[i])) command.push_back(s[i++]);
    if (command == "quad" || command == "qquad") return true;
    i = start;
    return false;
  }
  void ws() {
    while (i < s.size()) {
      if (std::isspace((unsigned char)s[i])) {
        ++i;
        continue;
      }
      if (eat_latex_space()) continue;
      break;
    }
  }
  bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }
  bool starts_atom() { ws(); if (i>=s.size() || starts_latex_command("right")) return false; char c=s[i]; return c=='(' || c=='\\' || c=='.' || std::isdigit((unsigned char)c) || std::isalpha((unsigned char)c); }
  [[noreturn]] void fail(const std::string& message) const { throw ParseError(message, i); }

  bool eat_latex_binary_operator(char& op) {
    ws();
    std::size_t start = i;
    if (i >= s.size() || s[i] != '\\') return false;
    ++i;
    std::string command;
    while (i < s.size() && std::isalpha((unsigned char)s[i])) command.push_back(s[i++]);
    if (command == "cdot" || command == "times") {
      op = '*';
      return true;
    }
    if (command == "div") {
      op = '/';
      return true;
    }
    i = start;
    return false;
  }

  std::string brace_content() {
    ws(); if (i >= s.size() || s[i] != '{') fail("expected {");
    ++i; int depth=1; std::size_t start=i;
    while (i < s.size() && depth>0) { if (s[i]=='{') ++depth; else if (s[i]=='}') --depth; ++i; }
    if (depth!=0) throw ParseError("unclosed brace", start - 1);
    return s.substr(start, (i-1)-start);
  }

  std::string bracket_content() {
    ws(); if (i >= s.size() || s[i] != '[') fail("expected [");
    ++i; int depth=1; std::size_t start=i;
    while (i < s.size() && depth>0) { if (s[i]=='[') ++depth; else if (s[i]==']') --depth; ++i; }
    if (depth!=0) throw ParseError("unclosed bracket", start - 1);
    return s.substr(start, (i-1)-start);
  }

  std::string latex_delimiter() {
    ws();
    if (i >= s.size()) fail("expected delimiter");
    if (s[i] != '\\') return std::string(1, s[i++]);
    ++i;
    if (i < s.size() && !std::isalpha((unsigned char)s[i])) return std::string(1, s[i++]);
    std::string command;
    while (i < s.size() && std::isalpha((unsigned char)s[i])) command.push_back(s[i++]);
    if (command == "lvert" || command == "rvert" || command == "vert") return "|";
    if (command == "lbrace") return "{";
    if (command == "rbrace") return "}";
    if (command == "langle") return "<";
    if (command == "rangle") return ">";
    fail("expected delimiter");
  }

  std::string right_delimiter() {
    ws();
    if (!starts_latex_command("right")) fail("missing \\right");
    i += 6;
    return latex_delimiter();
  }

  std::unique_ptr<Expr> number(){
    ws();
    if(i>=s.size()||(!std::isdigit((unsigned char)s[i])&&s[i]!='.')) fail("expected number");
    std::string text; bool dot=false; bool digit=false;
    while(i<s.size()&&(std::isdigit((unsigned char)s[i])||s[i]=='.')){
      if(s[i]=='.'){ if(dot) fail("multiple decimal points"); dot=true; }
      else digit=true;
      text.push_back(s[i++]);
    }
    if(!digit) fail("expected digit");
    if(dot) return std::make_unique<Decimal>(text, std::stold(text));
    long long v=0; for(char c:text) v=v*10+(c-'0');
    return std::make_unique<Number>(v);
  }
  std::string identifier(){ ws(); if(i>=s.size()||!std::isalpha((unsigned char)s[i])) fail("expected symbol"); std::string n; while(i<s.size()&&std::isalnum((unsigned char)s[i])) n.push_back(s[i++]); return n; }
  std::unique_ptr<Expr> symbol_or_function(){
    auto name=identifier();
    ws();
    if(eat('(')){
      auto arg=expr(); if(!eat(')')) fail("missing )");
      if(name=="sqrt") return std::make_unique<Sqrt>(std::move(arg));
      return std::make_unique<FunctionCall>(name,std::move(arg));
    }
    return std::make_unique<Symbol>(name);
  }
  std::unique_ptr<Expr> latex_frac(){ i+=5; P np(brace_content()); P dp(brace_content()); return std::make_unique<Fraction>(np.expr(),dp.expr()); }
  std::unique_ptr<Expr> latex_sqrt(){ i+=5; P rp(brace_content()); return std::make_unique<Sqrt>(rp.expr()); }
  std::unique_ptr<Expr> latex_command(){
    ws(); if(i>=s.size()||s[i]!='\\') fail("expected latex command");
    ++i; std::string command;
    while(i<s.size()&&std::isalpha((unsigned char)s[i])) command.push_back(s[i++]);
    if(command.empty()) fail("expected latex command name");
    if(command=="left"){
      auto left = latex_delimiter();
      auto inner = expr();
      auto right = right_delimiter();
      if(left=="|" && right=="|") return std::make_unique<FunctionCall>("abs",std::move(inner));
      return inner;
    }
    if(command=="right") throw ParseError("unexpected \\right without matching \\left", i - command.size() - 1);
    if(command=="frac"){ P np(brace_content()); P dp(brace_content()); return std::make_unique<Fraction>(np.expr(),dp.expr()); }
    if(command=="sqrt"){
      std::unique_ptr<Expr> index;
      ws();
      if(i<s.size()&&s[i]=='['){ P ip(bracket_content()); index=ip.expr(); }
      P rp(brace_content());
      return index ? std::make_unique<Sqrt>(rp.expr(), std::move(index)) : std::make_unique<Sqrt>(rp.expr());
    }
    if(command=="sin"||command=="cos"||command=="tan"||command=="ln"||command=="log"||command=="exp"||command=="abs"){
      std::unique_ptr<Expr> arg;
      ws();
      if(i<s.size()&&s[i]=='{'){ P ap(brace_content()); arg=ap.expr(); }
      else if(i<s.size()&&s[i]=='('){ ++i; arg=expr(); if(!eat(')')) fail("missing )"); }
      else arg=atom();
      return std::make_unique<FunctionCall>(command,std::move(arg));
    }
    if(auto symbol_name=latex_command_to_symbol(command)) return std::make_unique<Symbol>(*symbol_name);
    std::string message = "unknown latex command: \\" + command;
    if (auto suggestion = suggest_latex_command(command)) message += " (did you mean \\" + *suggestion + "?)";
    throw ParseError(message, i - command.size() - 1);
  }

  std::unique_ptr<Expr> atom(){
    ws();
    if (eat('-')) return std::make_unique<Binary>('*', std::make_unique<Number>(-1), atom());
    if (eat('(')) { auto e=expr(); if(!eat(')')) fail("missing )"); return e; }
    if (i<s.size() && s[i]=='\\') return latex_command();
    if (i<s.size() && (std::isdigit((unsigned char)s[i])||s[i]=='.')) return number();
    return symbol_or_function();
  }

  std::unique_ptr<Expr> postfix(){ auto base=atom(); while(true){ ws(); if(eat('^')){ std::unique_ptr<Expr> ex; ws(); if(i<s.size()&&s[i]=='{'){ P sp(brace_content()); ex=sp.expr(); } else ex=atom(); base=std::make_unique<Superscript>(std::move(base),std::move(ex)); }
      else if(eat('_')){ std::unique_ptr<Expr> sub; ws(); if(i<s.size()&&s[i]=='{'){ P sp(brace_content()); sub=sp.expr(); } else sub=atom(); base=std::make_unique<Subscript>(std::move(base),std::move(sub)); }
      else if(eat('!')){ base=std::make_unique<FunctionCall>("fact",std::move(base)); }
      else if(eat('%')){ base=std::make_unique<FunctionCall>("percent",std::move(base)); }
      else break; } return base; }

  std::unique_ptr<Expr> term(){ auto l=postfix(); while(true){ ws(); char latex_op=0; if(eat_latex_binary_operator(latex_op)){ auto r=postfix(); l=std::make_unique<Binary>(latex_op,std::move(l),std::move(r)); }
      else if(i<s.size()&&(s[i]=='*'||s[i]=='/')){ char op=s[i++]; auto r=postfix(); l=std::make_unique<Binary>(op,std::move(l),std::move(r)); }
      else if (starts_atom()) { auto r=postfix(); l=std::make_unique<Binary>('*', std::move(l), std::move(r)); }
      else break; } return l; }

  std::unique_ptr<Expr> expr(){ auto l=term(); while(true){ ws(); if(i>=s.size()||(s[i]!='+'&&s[i]!='-')) break; char op=s[i++]; auto r=term(); l=std::make_unique<Binary>(op,std::move(l),std::move(r)); } return l; }
};

}  // namespace

std::unique_ptr<Expr> parse(std::string input){ P p(std::move(input)); auto out=p.expr(); p.ws(); if(p.i!=p.s.size()) throw ParseError("unexpected trailing input", p.i); return out; }
} // namespace overcalc
