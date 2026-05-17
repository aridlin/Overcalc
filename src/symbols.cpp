#include "overcalc/symbols.hpp"

#include <array>
#include <algorithm>
#include <vector>

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
    SymbolInfo{"sum", "sum", "\xE2\x88\x91"},
    SymbolInfo{"prod", "prod", "\xE2\x88\x8F"},
    SymbolInfo{"int", "int", "\xE2\x88\xAB"},
};

std::size_t edit_distance(std::string_view a, std::string_view b) {
  std::vector<std::size_t> prev(b.size() + 1), cur(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = j;
  for (std::size_t i = 1; i <= a.size(); ++i) {
    cur[0] = i;
    for (std::size_t j = 1; j <= b.size(); ++j) {
      std::size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
    }
    std::swap(prev, cur);
  }
  return prev[b.size()];
}

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

std::optional<std::string> suggest_latex_command(std::string_view command) {
  std::size_t best_distance = 3;
  std::optional<std::string> best;
  for (const auto& symbol : symbols) {
    std::size_t distance = edit_distance(command, symbol.command);
    if (distance < best_distance) {
      best_distance = distance;
      best = std::string(symbol.command);
    }
  }
  return best;
}

std::string render_symbol(std::string_view name, bool ascii) {
  if (ascii) return std::string(name);
  for (const auto& symbol : symbols) {
    if (symbol.name == name) return std::string(symbol.unicode);
  }
  return std::string(name);
}

std::vector<SymbolListing> supported_symbols() {
  std::vector<SymbolListing> out;
  out.reserve(symbols.size());
  for (const auto& symbol : symbols) {
    out.push_back(SymbolListing{std::string(symbol.name), std::string(symbol.command), std::string(symbol.unicode)});
  }
  return out;
}

}  // namespace overcalc
