#include "overcalc/ast_dump.hpp"
#include "overcalc/eval.hpp"
#include "overcalc/latex.hpp"
#include "overcalc/parser.hpp"
#include "overcalc/render.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv){
  if(argc<2){ std::cerr<<"usage: overcalc [--ascii] [--latex-output] [--ast-json] [--steps] '<expr>'\n"; return 2;}
  overcalc::RenderOptions ropt; bool latex_output=false; bool ast_json=false; bool steps_on=false; std::string expr;
  for(int i=1;i<argc;++i){ std::string a=argv[i]; if(a=="--ascii") ropt.ascii=true; else if(a=="--latex-output") latex_output=true; else if(a=="--ast-json") ast_json=true; else if(a=="--steps") steps_on=true; else expr=a; }
  if(expr.empty()){ std::cerr<<"error: missing expression\n"; return 2; }
  try{
    auto ast=overcalc::parse(expr);
    if(ast_json){ std::cout<<overcalc::ast_to_json(*ast)<<"\n"; return 0; }
    overcalc::StepLog logs;
    auto r=overcalc::evaluate(*ast, steps_on?&logs:nullptr);
    std::string result=r.exact;
    if(r.decimal){ std::ostringstream o; o<<r.exact<<" ≈ "<<std::setprecision(12)<<(double)*r.decimal; result=o.str(); }
    if(latex_output){ auto normalized=overcalc::to_latex(*ast); std::cout<<normalized<<" = "<<r.exact; if(r.decimal) std::cout<<" \\approx "<<std::setprecision(12)<<(double)*r.decimal; std::cout<<"\n"; return 0; }
    std::cout<<overcalc::render_boxed(expr,result,ropt);
    if(steps_on){ std::cout << (ropt.ascii?"\n-- Steps --\n":"\n╭─ Steps ───────────────────────────────╮\n"); for(auto& e:logs.entries) std::cout<<(ropt.ascii?"* ":"│ ")<<e<<"\n"; if(!ropt.ascii) std::cout<<"╰───────────────────────────────────────╯\n"; }
    return 0;
  } catch(const std::exception& e){ std::cerr<<"error: "<<e.what()<<"\n"; return 1; }
}
