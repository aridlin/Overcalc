#include "overcalc/eval.hpp"
#include "overcalc/parser.hpp"
#include "overcalc/render.hpp"
#include <iostream>
int main(int argc, char** argv){ if(argc<2){ std::cerr<<"usage: overcalc '<expr>'\n"; return 2;} try{ auto ast=overcalc::parse(argv[1]); auto result=overcalc::evaluate_to_string(*ast); std::cout<<overcalc::render_boxed(argv[1],result); return 0; } catch(const std::exception& e){ std::cerr<<"error: "<<e.what()<<"\n"; return 1; } }
