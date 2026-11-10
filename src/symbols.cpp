#include "overcalc/symbols.hpp"

#include <array>

namespace overcalc {
namespace {

struct SymbolInfo {
  std::string_view name;
  std::string_view command;
  std::string_view unicode;
};

constexpr std::array symbols{
    SymbolInfo{"alpha", "alpha", "\xCE\xB1"},
    SymbolInfo{"beta", "beta", "\xCE\xB2"},
    SymbolInfo{"gamma", "gamma", "\xCE\xB3"},
    SymbolInfo{"delta", "delta", "\xCE\xB4"},
    SymbolInfo{"epsilon", "epsilon", "\xCE\xB5"},
    SymbolInfo{"zeta", "zeta", "\xCE\xB6"},
    SymbolInfo{"eta", "eta", "\xCE\xB7"},
    SymbolInfo{"theta", "theta", "\xCE\xB8"},
    SymbolInfo{"iota", "iota", "\xCE\xB9"},
    SymbolInfo{"kappa", "kappa", "\xCE\xBA"},
    SymbolInfo{"lambda", "lambda", "\xCE\xBB"},
    SymbolInfo{"mu", "mu", "\xCE\xBC"},
    SymbolInfo{"nu", "nu", "\xCE\xBD"},
    SymbolInfo{"xi", "xi", "\xCE\xBE"},
    SymbolInfo{"omicron", "omicron", "\xCE\xBF"},
    SymbolInfo{"pi", "pi", "\xCF\x80"},
    SymbolInfo{"rho", "rho", "\xCF\x81"},
    SymbolInfo{"sigma", "sigma", "\xCF\x83"},
    SymbolInfo{"tau", "tau", "\xCF\x84"},
    SymbolInfo{"upsilon", "upsilon", "\xCF\x85"},
    SymbolInfo{"phi", "phi", "\xCF\x86"},
    SymbolInfo{"chi", "chi", "\xCF\x87"},
    SymbolInfo{"psi", "psi", "\xCF\x88"},
    SymbolInfo{"omega", "omega", "\xCF\x89"},
    SymbolInfo{"Gamma", "Gamma", "\xCE\x93"},
    SymbolInfo{"Delta", "Delta", "\xCE\x94"},
    SymbolInfo{"Theta", "Theta", "\xCE\x98"},
    SymbolInfo{"Lambda", "Lambda", "\xCE\x9B"},
    SymbolInfo{"Xi", "Xi", "\xCE\x9E"},
    SymbolInfo{"Pi", "Pi", "\xCE\xA0"},
    SymbolInfo{"Sigma", "Sigma", "\xCE\xA3"},
    SymbolInfo{"Upsilon", "Upsilon", "\xCE\xA5"},
    SymbolInfo{"Phi", "Phi", "\xCE\xA6"},
    SymbolInfo{"Psi", "Psi", "\xCE\xA8"},
    SymbolInfo{"Omega", "Omega", "\xCE\xA9"},
    SymbolInfo{"infty", "infty", "\xE2\x88\x9E"},
};

}  // namespace

std::optional<std::string> latex_command_to_symbol(std::string_view command) {
  for (const auto& symbol : symbols) {
    if (symbol.command == command) return std::string(symbol.name);
  }
  return std::nullopt;
}

std::optional<std::string> symbol_to_latex_command(std::string_view name) {
  for (const auto& symbol : symbols) {
    if (symbol.name == name) return std::string(symbol.command);
  }
  return std::nullopt;
}

std::string render_symbol(std::string_view name, bool ascii) {
  if (ascii) return std::string(name);
  for (const auto& symbol : symbols) {
    if (symbol.name == name) return std::string(symbol.unicode);
  }
  return std::string(name);
}

}  // namespace overcalc
