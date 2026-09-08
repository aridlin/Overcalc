#include "overcalc/editor.hpp"
#include "overcalc/parser.hpp"
#include "overcalc/eval.hpp"
#include "overcalc/simplify.hpp"
#include <iostream>
#include <stdexcept>
using overcalc::ExpressionEditor;
void check(bool ok, const char* message) { if(!ok) throw std::runtime_error(message); }
void value(const ExpressionEditor& e, const char* expected) {
  check(e.complete(), "unexpected empty field");
  auto ast=overcalc::parse(e.source());
  auto actual=overcalc::evaluate(*overcalc::simplify(*ast)).exact;
  if(actual!=expected) throw std::runtime_error(e.source()+" expected "+expected+" got "+actual);
}
int main() {
  ExpressionEditor e;
  check(!e.complete(), "empty expression complete");
  e.insert(0); e.type('1'); e.tab(); e.type('2'); value(e,"1/2");
  auto denominator=e.caret(); e.vertical(-1); e.home(true); e.type('2'); value(e,"6");
  e.click(denominator.x,denominator.y); e.home(true); e.type('4'); value(e,"1/2");
  e.undo(); value(e,"6"); e.undo(true); value(e,"1/2");
  e.clear(); e.type('2'); e.type('^'); e.type('3'); value(e,"8");
  e.tab(); e.type('+'); e.type('1'); value(e,"9");
  e.clear(); e.insert(0); e.insert(2); e.type('9'); e.tab(); e.tab(); e.type('3'); value(e,"1");
  e.clear(); e.type('1'); e.type('2'); e.type('/'); e.type('4'); value(e,"3");
  e.tab(); e.erase(true); check(!e.complete(),"delete did not remove structure"); e.undo(); value(e,"3");
  e.clear(); e.insert(5); e.type('3'); e.tab(); e.type('8'); value(e,"2");
  e.clear(); e.insert(7); e.insert(12); e.type('/'); e.type('2'); value(e,"1");
  e.clear(); e.type('('); e.type('2'); e.type('+'); e.type('3'); e.type(')'); e.type('^'); e.type('2'); value(e,"25");
  for(std::size_t n=0;n<overcalc::edit_templates().size();++n) {
    ExpressionEditor t; t.insert(n);
    for(int f=0;f<overcalc::edit_templates()[n].fields;++f) {t.type(n==3 && f==0 ? 'x' : '2');t.tab();}
    // Standalone postfix operators need an operand; all other templates should parse.
    if(n!=14 && n!=15 && n!=16) { try { (void)overcalc::parse(t.source()); } catch (const std::exception& err) {throw std::runtime_error(t.source()+": "+err.what());} }
    auto layout=t.layout(); for(const auto& c:layout.carets)
      check(c.x>=0 && c.x<layout.width && c.y>=0 && c.y<static_cast<int>(layout.lines.size()),"caret outside layout");
  }
  e.clear(); e.insert(12); e.type('+'); e.insert(17);
  auto unicode=e.layout();
  check(unicode.lines[0].find(U"π + α")!=std::u32string::npos,"Greek or operator glyphs missing");
  check(unicode.width==static_cast<int>(unicode.lines[0].size()),"Unicode width uses bytes");
  auto at=e.caret(); e.click(at.x,at.y); e.type('2');
  check(e.source()=="pi+alpha2","Unicode click changed source offset");
  e.clear();e.insert(4);e.insert(0);e.type('1');e.tab();e.type('2');
  auto tall=e.layout();
  check(tall.lines.front()[0]==U'⎛' && tall.lines.back()[0]==U'⎝',"tall parentheses missing");
  check(overcalc::editor_utf8(U"π√─□")=="π√─□","UTF-8 encoding incorrect");
  std::cout << "Structured editing scenarios passed\n";
}
