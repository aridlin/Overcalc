#include "overcalc/render.hpp"
#include <sstream>
namespace overcalc { std::string render_boxed(const std::string& input,const std::string& result){ std::ostringstream o; o<<"OverCalc\n"<<"Input: "<<input<<"\n"<<"Result: "<<result<<"\n"; return o.str(); } }
