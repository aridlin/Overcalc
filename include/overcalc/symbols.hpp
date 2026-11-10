#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace overcalc {

std::optional<std::string> latex_command_to_symbol(std::string_view command);
std::optional<std::string> symbol_to_latex_command(std::string_view symbol);
std::string render_symbol(std::string_view symbol, bool ascii);

}  // namespace overcalc
