#include "overcalc/eval.hpp"
#include "overcalc/parser.hpp"
#include "overcalc/render.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv){
  if(argc<2){ std::cerr<<"usage: overcalc [--ascii] [--latex-output] '<expr>'\n"; return 2;}

  overcalc::RenderOptions ropt;
  bool latex_output = false;
  std::string expr;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--ascii") ropt.ascii = true;
    else if (a == "--latex-output") latex_output = true;
    else expr = a;
  }
  if (expr.empty()) { std::cerr << "error: missing expression\n"; return 2; }

  try{
    auto ast=overcalc::parse(expr);
    auto r = overcalc::evaluate(*ast);
    std::string result = r.exact;
    if (r.decimal.has_value()) {
      std::ostringstream o;
      o << r.exact << " ≈ " << std::setprecision(12) << static_cast<double>(*r.decimal);
      result = o.str();
    }
    if (latex_output) {
      std::cout << expr << " = " << r.exact;
      if (r.decimal.has_value()) std::cout << " \\approx " << std::setprecision(12) << static_cast<double>(*r.decimal);
      std::cout << "\n";
      return 0;
    }
    std::cout<<overcalc::render_boxed(expr, result, ropt);
    return 0;
  } catch(const std::exception& e){ std::cerr<<"error: "<<e.what()<<"\n"; return 1; }
}
