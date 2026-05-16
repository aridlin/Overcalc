#pragma once
#include <string>

namespace overcalc {

// Renders a TUI-style panelized output with pretty unicode math.
std::string render_boxed(const std::string& input, const std::string& result);

}
