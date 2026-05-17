#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace overcalc {

struct SymbolListing {
  std::string name;
  std::string command;
  std::string unicode;
};

std::optional<std::string> latex_command_to_symbol(std::string_view command);
std::optional<std::string> symbol_to_latex_command(std::string_view symbol);
std::optional<std::string> suggest_latex_command(std::string_view command);
std::string render_symbol(std::string_view symbol, bool ascii);
std::vector<SymbolListing> supported_symbols();

}  // namespace overcalc
