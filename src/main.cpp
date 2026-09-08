#include "overcalc/ast_dump.hpp"
#include "overcalc/editor.hpp"
#include "overcalc/derivative.hpp"
#include "overcalc/diagnostics.hpp"
#include "overcalc/eval.hpp"
#include "overcalc/format.hpp"
#include "overcalc/latex.hpp"
#include "overcalc/parser.hpp"
#include "overcalc/render.hpp"
#include "overcalc/simplify.hpp"
#include "overcalc/symbols.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <io.h>
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif
#ifndef ENABLE_QUICK_EDIT_MODE
#define ENABLE_QUICK_EDIT_MODE 0x0040
#endif
#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_EXTENDED_FLAGS 0x0080
#endif
#else
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {

constexpr const char* kVersion = "0.1.0";
constexpr int kTuiInputRow = 4;
constexpr int kTuiSuggestionRow = 6;
constexpr int kTuiPreviewStartRow = 9;
constexpr int kTuiPrettyContentStartRow = 14;
constexpr int kTuiInputPrefixWidth = 2;

bool should_show_decimal(const std::string& exact, long double decimal);
bool is_exact_numeric(const std::string& text);

std::string usage_text() {
  return
      "usage: overcalc [options] '<expr>'\n"
      "\n"
      "Options:\n"
      "  --ascii             Use ASCII rendering\n"
      "  --unicode           Use Unicode rendering (default)\n"
      "  --no-color          Disable ANSI colors\n"
      "  --width N           Set panel width\n"
      "  --latex-output      Print normalized LaTeX and result\n"
      "  --json              Print structured result JSON\n"
      "  --ast-json          Print AST JSON\n"
      "  --timing            Print elapsed execution time\n"
      "  --batch             Evaluate one expression per input line\n"
      "  --tui               Start interactive editor/preview mode\n"
      "  --derive VAR        Differentiate with respect to VAR\n"
      "  --steps             Show evaluation steps\n"
      "  --file PATH         Read expression from a file\n"
      "  --stdin, -          Read expression from standard input\n"
      "  --list-symbols      List supported LaTeX symbols\n"
      "  --version           Print version\n"
      "  --help, -h          Show this help\n"
      "\n"
      "Examples:\n"
      "  overcalc \"((2 + 3)^2 - 9) / 4\"\n"
      "  overcalc --tui\n"
      "  overcalc --steps \"sin(pi/2)+50%\"\n"
      "  overcalc --batch --file formulas.txt\n"
      "  overcalc --file formula.txt\n";
}

std::string join_args(const std::vector<std::string>& parts) {
  std::ostringstream out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out << ' ';
    out << parts[i];
  }
  return out.str();
}

std::string json_string(const std::string& text) {
  std::ostringstream out;
  out << '"';
  for (unsigned char c : text) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c)
              << std::dec << std::setfill(' ');
        } else {
          out << static_cast<char>(c);
        }
        break;
    }
  }
  out << '"';
  return out.str();
}

std::string decimal_string(long double value) {
  std::ostringstream out;
  out << std::setprecision(12) << static_cast<double>(value);
  return out.str();
}

std::string result_text(const overcalc::EvalResult& result) {
  std::ostringstream out;
  out << (is_exact_numeric(result.exact) ? "Exact    " : "Form     ") << result.exact;
  if (result.decimal && should_show_decimal(result.exact, *result.decimal)) {
    out << "\nDecimal  " << decimal_string(*result.decimal);
  }
  return out.str();
}

std::string trim_copy(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}

std::vector<std::pair<int, std::string>> batch_lines(const std::string& input) {
  std::vector<std::pair<int, std::string>> out;
  std::istringstream stream(input);
  std::string line;
  int line_no = 0;
  while (std::getline(stream, line)) {
    ++line_no;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    line = trim_copy(line);
    if (line.empty() || line[0] == '#') continue;
    out.push_back({line_no, line});
  }
  return out;
}

double elapsed_ms(std::chrono::steady_clock::time_point start) {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now() - start).count();
}

struct RunResult {
  int line = 0;
  std::string input;
  bool ok = false;
  std::string latex;
  std::string simplified;
  std::string simplified_latex;
  std::string derivative;
  std::string derivative_latex;
  std::string exact;
  std::optional<long double> decimal;
  std::vector<std::string> steps;
  std::string error;
  double timing_ms = 0.0;
};

RunResult run_expression(const std::string& input, int line, bool collect_steps, const std::string& derive_var = "") {
  RunResult out;
  out.line = line;
  out.input = input;
  auto start = std::chrono::steady_clock::now();
  try {
    auto ast = overcalc::parse(input);
    auto simplified = overcalc::simplify(*ast);
    std::unique_ptr<overcalc::Expr> derivative_expr;
    if (!derive_var.empty()) {
      derivative_expr = overcalc::simplify(*overcalc::derivative(*simplified, derive_var));
      simplified = overcalc::clone_expr(*derivative_expr);
    }
    std::string original_text = overcalc::to_infix(*ast);
    std::string simplified_text = overcalc::to_infix(*simplified);
    overcalc::StepLog logs;
    auto result = overcalc::evaluate(*simplified, collect_steps ? &logs : nullptr);
    if (collect_steps && !derive_var.empty()) {
      logs.entries.insert(logs.entries.begin() + std::min<std::size_t>(1, logs.entries.size()),
                          "Derivative d/d" + derive_var + ": " + simplified_text);
    } else if (collect_steps && simplified_text != original_text) {
      logs.entries.insert(logs.entries.begin() + std::min<std::size_t>(1, logs.entries.size()),
                          "Simplified: " + simplified_text);
    }
    out.ok = true;
    out.latex = overcalc::to_latex(*ast);
    out.simplified = simplified_text;
    out.simplified_latex = overcalc::to_latex(*simplified);
    if (derivative_expr) {
      out.derivative = overcalc::to_infix(*derivative_expr);
      out.derivative_latex = overcalc::to_latex(*derivative_expr);
    }
    out.exact = result.exact;
    out.decimal = result.decimal;
    out.steps = std::move(logs.entries);
  } catch (const overcalc::ParseError& err) {
    std::ostringstream message;
    message << "parse error at column " << (err.offset() + 1) << ": " << err.what();
    out.error = message.str();
  } catch (const std::exception& err) {
    out.error = err.what();
  }
  out.timing_ms = elapsed_ms(start);
  return out;
}

void print_timing(double ms) {
  std::cerr << "timing_ms " << std::fixed << std::setprecision(3) << ms << "\n";
}

void write_json_steps(std::ostream& out, const std::vector<std::string>& steps, bool include_steps) {
  out << "[";
  if (include_steps) {
    for (size_t i = 0; i < steps.size(); ++i) {
      if (i) out << ", ";
      out << json_string(steps[i]);
    }
  }
  out << "]";
}

void write_run_json(std::ostream& out, const RunResult& result, bool include_steps, bool include_timing,
                    const std::string& indent) {
  out << indent << "{\n";
  out << indent << "  \"line\": " << result.line << ",\n";
  out << indent << "  \"input\": " << json_string(result.input) << ",\n";
  out << indent << "  \"ok\": " << (result.ok ? "true" : "false");
  if (result.ok) {
    out << ",\n" << indent << "  \"latex\": " << json_string(result.latex) << ",\n";
    out << indent << "  \"simplified\": " << json_string(result.simplified) << ",\n";
    out << indent << "  \"simplified_latex\": " << json_string(result.simplified_latex) << ",\n";
    if (!result.derivative.empty()) {
      out << indent << "  \"derivative\": " << json_string(result.derivative) << ",\n";
      out << indent << "  \"derivative_latex\": " << json_string(result.derivative_latex) << ",\n";
    }
    out << indent << "  \"exact\": " << json_string(result.exact) << ",\n";
    out << indent << "  \"decimal\": ";
    if (result.decimal) out << decimal_string(*result.decimal);
    else out << "null";
    out << ",\n" << indent << "  \"steps\": ";
    write_json_steps(out, result.steps, include_steps);
  } else {
    out << ",\n" << indent << "  \"error\": " << json_string(result.error);
  }
  if (include_timing) {
    out << ",\n" << indent << "  \"timing_ms\": " << std::fixed << std::setprecision(3) << result.timing_ms;
  }
  out << "\n" << indent << "}";
}

void print_result_json(const std::string& input, const overcalc::Expr& ast, const overcalc::Expr& simplified,
                       const overcalc::EvalResult& result, const overcalc::StepLog& logs, bool include_steps,
                       std::optional<double> timing_ms, const std::string& derivative_text = "",
                       const std::string& derivative_latex = "") {
  std::vector<std::string> steps = logs.entries;
  RunResult run;
  run.line = 1;
  run.input = input;
  run.ok = true;
  run.latex = overcalc::to_latex(ast);
  run.simplified = overcalc::to_infix(simplified);
  run.simplified_latex = overcalc::to_latex(simplified);
  run.derivative = derivative_text;
  run.derivative_latex = derivative_latex;
  run.exact = result.exact;
  run.decimal = result.decimal;
  run.steps = std::move(steps);
  if (timing_ms) run.timing_ms = *timing_ms;
  write_run_json(std::cout, run, include_steps, timing_ms.has_value(), "");
  std::cout << "\n";
}

int print_batch(const std::string& input, bool json_output, bool steps_on, bool timing_on,
                const std::string& derive_var) {
  auto lines = batch_lines(input);
  if (lines.empty()) {
    std::cerr << "error: batch input had no expressions\n";
    return 2;
  }

  std::vector<RunResult> results;
  results.reserve(lines.size());
  bool all_ok = true;
  auto batch_start = std::chrono::steady_clock::now();
  for (const auto& [line_no, expr] : lines) {
    results.push_back(run_expression(expr, line_no, steps_on || json_output, derive_var));
    all_ok = all_ok && results.back().ok;
  }
  double total_ms = elapsed_ms(batch_start);

  if (json_output) {
    std::cout << "[\n";
    for (size_t i = 0; i < results.size(); ++i) {
      if (i) std::cout << ",\n";
      write_run_json(std::cout, results[i], steps_on, timing_on, "  ");
    }
    std::cout << "\n]\n";
  } else {
    for (const auto& result : results) {
      std::cout << result.line << "  " << result.input << "  ";
      if (!result.ok) {
        std::cout << "error: " << result.error << "\n";
        continue;
      }
      if (!result.simplified.empty() && result.simplified != result.exact && result.simplified_latex != result.latex) {
        if (!result.derivative.empty()) std::cout << "=> d/d" << derive_var << " " << result.derivative << "  ";
        else std::cout << "=> " << result.simplified << "  ";
      }
      std::cout << "= " << result.exact;
      if (result.decimal && should_show_decimal(result.exact, *result.decimal)) {
        std::cout << " ~= " << decimal_string(*result.decimal);
      }
      std::cout << "\n";
      if (steps_on) {
        for (const auto& step : result.steps) std::cout << "   * " << step << "\n";
      }
    }
  }
  if (timing_on) {
    print_timing(total_ms);
  }
  return all_ok ? 0 : 1;
}

std::string read_stdin() {
  std::ostringstream out;
  out << std::cin.rdbuf();
  return out.str();
}

std::string read_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error("could not open file: " + path);
  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

std::string stderr_ansi(const std::string& text, const std::string& code, const overcalc::RenderOptions& opt) {
  return opt.color ? "\x1b[" + code + "m" + text + "\x1b[0m" : text;
}

std::string stdout_ansi(const std::string& text, const std::string& code, const overcalc::RenderOptions& opt) {
  return opt.color ? "\x1b[" + code + "m" + text + "\x1b[0m" : text;
}

void print_supported_symbols(const overcalc::RenderOptions& opt) {
  std::cout << stdout_ansi("Supported LaTeX Symbols", "36;1", opt) << "\n";
  for (const auto& symbol : overcalc::supported_symbols()) {
    std::cout << "  " << stdout_ansi("\\" + symbol.command, "36;1", opt)
              << std::string(std::max(1, 14 - static_cast<int>(symbol.command.size())), ' ')
              << stdout_ansi(symbol.name, "35;1", opt)
              << std::string(std::max(1, 12 - static_cast<int>(symbol.name.size())), ' ')
              << symbol.unicode << "\n";
  }
}

struct Completion {
  std::string label;
  std::string insert;
  std::string description;
  std::size_t caret = overcalc::npos;
};

std::vector<Completion> base_completions() {
  std::vector<Completion> out = {
      {"\\frac", "\\frac{}{}", "fraction template", 6},
      {"\\sqrt", "\\sqrt{}", "square root template", 6},
      {"\\sqrt[3]", "\\sqrt[3]{}", "indexed root template", 9},
      {"\\left(\\right)", "\\left(\\right)", "stretchy parentheses", 6},
      {"\\left|\\right|", "\\left|\\right|", "absolute value", 6},
      {"sin", "sin()", "sine function", 4},
      {"cos", "cos()", "cosine function", 4},
      {"tan", "tan()", "tangent function", 4},
      {"ln", "ln()", "natural log", 3},
      {"exp", "exp()", "exponential", 4},
      {"abs", "abs()", "absolute value", 4},
      {"x^2", "x^2", "power template"},
      {"x_0", "x_0", "subscript template"},
      {"\\sum", "\\sum_i^n", "large summation"},
      {"\\prod", "\\prod_i^n", "large product"},
      {"\\int", "\\int_0^1", "integral symbol"}};
  for (const auto& symbol : overcalc::supported_symbols()) {
    out.push_back({"\\" + symbol.command, "\\" + symbol.command, symbol.name + " " + symbol.unicode});
  }
  return out;
}

std::string current_token(const std::string& input, std::size_t cursor) {
  cursor = std::min(cursor, input.size());
  if (cursor == 0) return "";
  std::size_t pos = cursor;
  while (pos > 0) {
    unsigned char c = static_cast<unsigned char>(input[pos - 1]);
    if (!(std::isalnum(c) || input[pos - 1] == '\\' || input[pos - 1] == '_')) break;
    --pos;
  }
  return input.substr(pos, cursor - pos);
}

std::vector<Completion> completions_for(const std::string& input, std::size_t cursor) {
  std::string token = current_token(input, cursor);
  auto all = base_completions();
  std::vector<Completion> out;
  if (token.empty()) {
    for (size_t i = 0; i < std::min<std::size_t>(8, all.size()); ++i) out.push_back(all[i]);
    return out;
  }
  for (const auto& item : all) {
    if (item.label.rfind(token, 0) == 0 || item.insert.rfind(token, 0) == 0) out.push_back(item);
    if (out.size() >= 8) break;
  }
  return out;
}

void apply_completion(std::string& input, std::size_t& cursor, const Completion& completion) {
  cursor = std::min(cursor, input.size());
  std::string token = current_token(input, cursor);
  std::size_t start = cursor - token.size();
  if (!token.empty()) input.erase(start, token.size());
  input.insert(start, completion.insert);
  cursor = start + (completion.caret == overcalc::npos ? completion.insert.size() : completion.caret);
}

bool convert_inline_fraction_on_space(std::string& input, std::size_t& cursor) {
  cursor = std::min(cursor, input.size());
  std::size_t start = cursor;
  while (start > 0) {
    char c = input[start - 1];
    if (std::isspace(static_cast<unsigned char>(c)) || c == '(' || c == ')' || c == '+' || c == '-' ||
        c == '*' || c == '=') {
      break;
    }
    --start;
  }
  std::string token = input.substr(start, cursor - start);
  auto slash = token.find('/');
  if (slash == std::string::npos || token.find('/', slash + 1) != std::string::npos) return false;
  std::string num = token.substr(0, slash);
  std::string den = token.substr(slash + 1);
  if (num.empty() || den.empty()) return false;
  std::string replacement = "\\frac{" + num + "}{" + den + "}";
  input.replace(start, token.size(), replacement);
  cursor = start + replacement.size();
  return true;
}

std::string completion_ghost_for(const std::string& input, std::size_t cursor) {
  auto suggestions = completions_for(input, cursor);
  if (suggestions.empty()) return "";
  std::string token = current_token(input, cursor);
  if (token.empty()) return "";
  const std::string& insert = suggestions.front().insert;
  if (!token.empty() && insert.rfind(token, 0) == 0) return insert.substr(token.size());
  return insert;
}

int visible_width(const std::string& text) {
  int width = 0;
  for (std::size_t i = 0; i < text.size();) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') {
      i += 2;
      while (i < text.size() && text[i] != 'm') ++i;
      if (i < text.size()) ++i;
      continue;
    }
    if ((c & 0xC0) != 0x80) ++width;
    ++i;
  }
  return width;
}

std::vector<std::string> split_output_lines(const std::string& text) {
  std::vector<std::string> lines;
  std::string line;
  for (char c : text) {
    if (c == '\n') {
      lines.push_back(line);
      line.clear();
    } else {
      line.push_back(c);
    }
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

std::string pad_visible(std::string text, int width) {
  text += std::string(std::max(0, width - visible_width(text)), ' ');
  return text;
}

std::string clip_visible(std::string text, int width) {
  std::string out;
  int used = 0;
  for (std::size_t i = 0; i < text.size() && used < width;) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') {
      std::size_t start = i;
      i += 2;
      while (i < text.size() && text[i] != 'm') ++i;
      if (i < text.size()) ++i;
      out.append(text, start, i - start);
      continue;
    }
    std::size_t len = 1;
    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    if (i + len > text.size()) len = 1;
    out.append(text, i, len);
    i += len;
    ++used;
  }
  return out;
}

std::vector<int> tui_source_depths(const std::string& input) {
  std::vector<int> depths(input.size(), 0);
  int group_depth = 0;
  int script_depth = 0;
  for (std::size_t i = 0; i < input.size(); ++i) {
    char c = input[i];
    if (c == ')' || c == ']' || c == '}') {
      group_depth = std::max(0, group_depth - 1);
      script_depth = std::max(0, script_depth - 1);
    }
    depths[i] = std::max(0, group_depth + script_depth);
    if (c == '(' || c == '[' || c == '{') ++group_depth;
    if (c == '^' || c == '_') ++script_depth;
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == ',' || c == '=') script_depth = 0;
  }
  return depths;
}

std::string tui_depth_color(int depth) {
  static const char* palette[] = {
      "38;5;229;1", "38;5;159;1", "38;5;219;1", "38;5;191;1",
      "38;5;215;1", "38;5;117;1", "38;5;183;1", "38;5;156;1"};
  constexpr std::size_t palette_size = sizeof(palette) / sizeof(palette[0]);
  return palette[static_cast<std::size_t>(std::max(0, depth)) % palette_size];
}

std::string tui_color_for_char(const std::string& input, std::size_t pos, int depth) {
  char c = input[pos];
  if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return tui_depth_color(depth);
  if (c == '\\') return "36;1";
  if (std::isalpha(static_cast<unsigned char>(c))) {
    std::size_t start = pos;
    while (start > 0 && std::isalpha(static_cast<unsigned char>(input[start - 1]))) --start;
    std::size_t end = pos;
    while (end < input.size() && std::isalpha(static_cast<unsigned char>(input[end]))) ++end;
    std::string word = input.substr(start, end - start);
    if (word == "sin" || word == "cos" || word == "tan" || word == "ln" || word == "log" ||
        word == "exp" || word == "abs" || word == "sqrt") {
      return "36;1";
    }
    return tui_depth_color(depth);
  }
  if (c == '^') return "95;1";
  if (c == '_') return "38;5;208;1";
  if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=' || c == '!' || c == '%') return "33;1";
  if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '|') return tui_depth_color(depth);
  return "37";
}

std::string highlighted_tui_input(const std::string& input, const overcalc::RenderOptions& opt) {
  if (input.empty()) return " ";

  std::ostringstream out;
  auto depths = tui_source_depths(input);
  for (std::size_t i = 0; i < input.size(); ++i) {
    std::string ch(1, input[i]);
    out << stdout_ansi(ch, tui_color_for_char(input, i, depths[i]), opt);
  }
  return out.str();
}

void print_tui_help() {
  std::cout
      << "Commands: :help  :symbols  :clear  :derive x  :derive off  :quit\n"
      << "Keys: arrows move, Backspace/Delete edit, Tab completes, Enter evaluates, Esc exits\n"
      << "Mouse: click the input row to place the caret; click a suggestion to insert it; click the preview to jump nearby\n";
}

std::string tui_preview(const std::string& input, const std::string& derive_var, const overcalc::RenderOptions& opt,
                        std::optional<std::size_t> cursor = std::nullopt) {
  if (trim_copy(input).empty()) return "";
  auto run = run_expression(input, 1, true, derive_var);
  if (!run.ok) {
    try {
      (void)overcalc::parse(input);
      return overcalc::render_boxed(input, "Math error  " + run.error, opt, "", "Simplified", cursor);
    } catch (const overcalc::ParseError&) {
      return overcalc::render_boxed(input, "Editing  incomplete expression", opt, "", "Simplified", cursor);
    } catch (...) {
      return overcalc::render_boxed(input, "Editing  incomplete expression", opt, "", "Simplified", cursor);
    }
  }
  std::string extra;
  std::string title = "Simplified";
  if (!derive_var.empty()) {
    extra = run.derivative;
    title = "Derivative d/d" + derive_var;
  } else if (!run.simplified.empty() && run.simplified_latex != run.latex) {
    extra = run.simplified;
  }
  return overcalc::render_boxed(input, result_text({run.exact, run.decimal}), opt, extra, title, cursor);
}

std::optional<std::size_t> suggestion_index_at_column(const std::string& input, std::size_t cursor, int column) {
  auto suggestions = completions_for(input, cursor);
  int pos = 14;
  for (std::size_t i = 0; i < suggestions.size(); ++i) {
    int start = pos;
    int end = start + static_cast<int>(suggestions[i].label.size()) - 1;
    if (column >= start && column <= end) return i;
    pos = end + 3;
  }
  return std::nullopt;
}

std::vector<std::string> make_tui_frame(const std::string& input, std::size_t cursor, const std::string& status,
                                        const std::string& derive_var, const overcalc::RenderOptions& opt) {
  std::vector<std::string> lines;
  int width = std::clamp(opt.panel_width, 48, 140);
  int inner = width - 4;
  std::string h = opt.ascii ? "-" : "\xE2\x94\x80";
  std::string tl = opt.ascii ? "+" : "\xE2\x95\xAD";
  std::string tr = opt.ascii ? "+" : "\xE2\x95\xAE";
  std::string bl = opt.ascii ? "+" : "\xE2\x95\xB0";
  std::string br = opt.ascii ? "+" : "\xE2\x95\xAF";
  std::string v = opt.ascii ? "|" : "\xE2\x94\x82";

  lines.push_back(stdout_ansi("OverCalc Interactive", "36;1", opt) +
                  stdout_ansi("  live equation preview + mouse editing", "90", opt));
  lines.push_back("Mode: " + (derive_var.empty() ? std::string("evaluate") : "derive d/d" + derive_var));
  cursor = std::min(cursor, input.size());
  std::string ghost = completion_ghost_for(input, cursor);
  std::string content = highlighted_tui_input(input, opt);
  if (!ghost.empty()) content += stdout_ansi(ghost, "90", opt);
  std::string top = tl + h + " Input ";
  for (int i = visible_width(top); i < width - 1; ++i) top += h;
  top += tr;
  std::string bottom = bl;
  for (int i = 0; i < width - 2; ++i) bottom += h;
  bottom += br;
  lines.push_back(stdout_ansi(top, "36", opt));
  content = pad_visible(clip_visible(content, inner), inner);
  lines.push_back(stdout_ansi(v, "36", opt) + " " + content + " " +
                  stdout_ansi(v, "36", opt));
  lines.push_back(stdout_ansi(bottom, "36", opt));
  auto suggestions = completions_for(input, cursor);
  std::ostringstream suggestions_line;
  suggestions_line << stdout_ansi("Suggestions:", "33;1", opt) << " ";
  if (!suggestions.empty()) {
    for (const auto& item : suggestions) suggestions_line << item.label << "  ";
  }
  lines.push_back(clip_visible(suggestions_line.str(), width));
  lines.push_back(clip_visible(stdout_ansi(status, "90", opt), width));
  lines.push_back("");
  auto preview = split_output_lines(tui_preview(input, derive_var, opt, cursor));
  lines.insert(lines.end(), preview.begin(), preview.end());
  return lines;
}

std::size_t tui_utf8_len(unsigned char c) {
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

std::vector<std::string> split_ansi_cells(const std::string& line) {
  std::vector<std::string> cells;
  std::string active_sgr;
  for (std::size_t i = 0; i < line.size();) {
    unsigned char c = static_cast<unsigned char>(line[i]);
    if (c == '\x1b' && i + 1 < line.size() && line[i + 1] == '[') {
      std::size_t start = i;
      i += 2;
      while (i < line.size() && line[i] != 'm') ++i;
      if (i < line.size()) ++i;
      std::string seq = line.substr(start, i - start);
      active_sgr = seq == "\x1b[0m" ? std::string() : seq;
      continue;
    }
    std::size_t len = tui_utf8_len(c);
    if (i + len > line.size()) len = 1;
    std::string cell = line.substr(i, len);
    if (!active_sgr.empty()) cell = active_sgr + cell + "\x1b[0m";
    cells.push_back(std::move(cell));
    i += len;
  }
  return cells;
}

std::string join_cells_from(const std::vector<std::string>& cells, std::size_t start) {
  std::string out, previous;
  for(std::size_t i=start;i<cells.size();++i) {
    const auto& cell=cells[i];
    std::string style,glyph=cell;
    if(cell.rfind("\x1b[",0)==0) {
      auto end=cell.find('m');
      style=cell.substr(0,end+1);glyph=cell.substr(end+1);
      if(glyph.ends_with("\x1b[0m")) glyph.resize(glyph.size()-4);
    }
    if(style!=previous) {out+=style.empty()?"\x1b[0m":style;previous=style;}
    out+=glyph;
  }
  return out;
}

class TuiScreen {
 public:
  void invalidate() { previous_cells_.clear(); }
  void draw(const std::vector<std::string>& lines, std::optional<std::pair<int, int>> cursor = std::nullopt) {
    std::vector<std::vector<std::string>> cells;
    cells.reserve(lines.size());
    for (const auto& line : lines) cells.push_back(split_ansi_cells(line));

    if (previous_cells_.empty()) {
      std::cout << "\x1b[2J\x1b[H";
      for (std::size_t row = 0; row < lines.size(); ++row)
        std::cout << "\x1b[" << row + 1 << ";1H" << lines[row] << "\x1b[K";
    } else {
      std::size_t row_count = std::max(cells.size(), previous_cells_.size());
      for (std::size_t row = 0; row < row_count; ++row) {
        const std::vector<std::string> empty;
        const auto& now = row < cells.size() ? cells[row] : empty;
        const auto& before = row < previous_cells_.size() ? previous_cells_[row] : empty;
        std::size_t first = 0;
        while (first < now.size() && first < before.size() && now[first] == before[first]) ++first;
        if (first == now.size() && first == before.size()) continue;
        std::cout << "\x1b[" << (row + 1) << ";" << (first + 1) << "H"
                  << join_cells_from(now, first) << "\x1b[0m\x1b[K";
      }
    }
    if (cursor) {
      std::cout << "\x1b[6 q\x1b[?25h"
                << "\x1b[" << cursor->second << ";" << cursor->first << "H";
    }
    previous_cells_ = std::move(cells);
    std::cout.flush();
  }

 private:
  std::vector<std::vector<std::string>> previous_cells_;
};

void draw_tui(TuiScreen& screen, const std::string& input, std::size_t cursor, const std::string& status,
              const std::string& derive_var, const overcalc::RenderOptions& opt) {
  int inner = std::clamp(opt.panel_width, 48, 140) - 4;
  int source_col = std::clamp(static_cast<int>(std::min(cursor, input.size())), 0, inner - 1);
  screen.draw(make_tui_frame(input, cursor, status, derive_var, opt),
              std::make_pair(kTuiInputPrefixWidth + 1 + source_col, kTuiInputRow));
}

bool stdin_is_terminal() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

enum class TuiEventType {
  Character,
  Backspace,
  DeleteKey,
  Enter,
  Tab,
  BackTab,
  Up,
  Down,
  Escape,
  CtrlC,
  Left,
  Right,
  Home,
  End,
  MousePress,
  Unknown
};

struct TuiEvent {
  TuiEventType type = TuiEventType::Unknown;
  char ch = 0;
  int x = 0;
  int y = 0;
};

int read_raw_char_blocking() {
#ifdef _WIN32
  return _getch();
#else
  char ch = 0;
  if (read(STDIN_FILENO, &ch, 1) != 1) return -1;
  return static_cast<unsigned char>(ch);
#endif
}

bool input_available_now() {
#ifdef _WIN32
  for (int i = 0; i < 25; ++i) {
    if (_kbhit() != 0) return true;
    Sleep(1);
  }
  return false;
#else
  fd_set set;
  FD_ZERO(&set);
  FD_SET(STDIN_FILENO, &set);
  timeval timeout{0, 25000};
  return select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout) > 0;
#endif
}

std::optional<int> read_raw_char_if_available() {
  if (!input_available_now()) return std::nullopt;
  int ch = read_raw_char_blocking();
  if (ch < 0) return std::nullopt;
  return ch;
}

std::optional<int> read_number_until(char stop, std::string& consumed) {
  std::string digits;
  while (true) {
    auto next = read_raw_char_if_available();
    if (!next) return std::nullopt;
    consumed.push_back(static_cast<char>(*next));
    if (*next == stop) break;
    if (!std::isdigit(static_cast<unsigned char>(*next))) return std::nullopt;
    if (digits.size() >= 6) return std::nullopt;
    digits.push_back(static_cast<char>(*next));
  }
  if (digits.empty()) return std::nullopt;
  return std::stoi(digits);
}

TuiEvent read_escape_event() {
  auto bracket = read_raw_char_if_available();
  if (!bracket) return {TuiEventType::Escape};
  if (*bracket != '[') return {TuiEventType::Unknown};

  auto next = read_raw_char_if_available();
  if (!next) return {TuiEventType::Escape};
  switch (*next) {
    case 'A': return {TuiEventType::Up};
    case 'B': return {TuiEventType::Down};
    case 'Z': return {TuiEventType::BackTab};
    case 'D': return {TuiEventType::Left};
    case 'C': return {TuiEventType::Right};
    case 'H': return {TuiEventType::Home};
    case 'F': return {TuiEventType::End};
    case '3': {
      auto tilde = read_raw_char_if_available();
      return tilde && *tilde == '~' ? TuiEvent{TuiEventType::DeleteKey} : TuiEvent{TuiEventType::Unknown};
    }
    case '<': {
      std::string consumed;
      auto button = read_number_until(';', consumed);
      auto x = read_number_until(';', consumed);
      std::string digits;
      int final = 0;
      while (auto ch = read_raw_char_if_available()) {
        if (*ch == 'M' || *ch == 'm') { final = *ch; break; }
        if (!std::isdigit(static_cast<unsigned char>(*ch)) || digits.size() > 6) return {TuiEventType::Unknown};
        digits.push_back(static_cast<char>(*ch));
      }
      if (!button || !x || digits.empty() || final != 'M' || *button != 0) return {TuiEventType::Unknown};
      return {TuiEventType::MousePress, 0, *x, std::stoi(digits)};
    }
    default:
      return {TuiEventType::Unknown};
  }
}

#ifdef _WIN32
std::optional<wchar_t> read_windows_key_char(HANDLE in, DWORD timeout_ms) {
  DWORD waited = 0;
  while (waited <= timeout_ms) {
    DWORD available = 0;
    if (!GetNumberOfConsoleInputEvents(in, &available)) return std::nullopt;
    if (available == 0) {
      Sleep(1);
      ++waited;
      continue;
    }
    INPUT_RECORD record{};
    DWORD read = 0;
    if (!ReadConsoleInputW(in, &record, 1, &read) || read == 0) return std::nullopt;
    if (record.EventType != KEY_EVENT) continue;
    const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
    if (!key.bKeyDown || key.uChar.UnicodeChar == 0) continue;
    return key.uChar.UnicodeChar;
  }
  return std::nullopt;
}

std::optional<int> read_windows_number_until(HANDLE in, wchar_t stop) {
  std::string digits;
  while (true) {
    auto ch = read_windows_key_char(in, 80);
    if (!ch) return std::nullopt;
    if (*ch == stop) break;
    if (*ch < L'0' || *ch > L'9') return std::nullopt;
    digits.push_back(static_cast<char>(*ch));
  }
  if (digits.empty()) return std::nullopt;
  return std::stoi(digits);
}

TuiEvent read_windows_escape_event(HANDLE in) {
  auto bracket = read_windows_key_char(in, 80);
  if (!bracket) return {TuiEventType::Escape};
  if (*bracket != L'[') return {TuiEventType::Unknown};
  auto next = read_windows_key_char(in, 80);
  if (!next) return {TuiEventType::Escape};
  switch (*next) {
    case L'A': return {TuiEventType::Up};
    case L'B': return {TuiEventType::Down};
    case L'Z': return {TuiEventType::BackTab};
    case L'D': return {TuiEventType::Left};
    case L'C': return {TuiEventType::Right};
    case L'H': return {TuiEventType::Home};
    case L'F': return {TuiEventType::End};
    case L'<': {
      auto button = read_windows_number_until(in, L';');
      auto x = read_windows_number_until(in, L';');
      std::string y_digits;
      wchar_t final = 0;
      while (true) {
        auto ch = read_windows_key_char(in, 80);
        if (!ch) return {TuiEventType::Unknown};
        if (*ch == L'M' || *ch == L'm') {
          final = *ch;
          break;
        }
        if (*ch < L'0' || *ch > L'9') return {TuiEventType::Unknown};
        y_digits.push_back(static_cast<char>(*ch));
      }
      if (!button || !x || y_digits.empty() || final != L'M') return {TuiEventType::Unknown};
      if ((*button & 3) != 0) return {TuiEventType::Unknown};
      return {TuiEventType::MousePress, 0, *x, std::stoi(y_digits)};
    }
    case L'3': {
      auto tilde = read_windows_key_char(in, 80);
      return tilde && *tilde == L'~' ? TuiEvent{TuiEventType::DeleteKey} : TuiEvent{TuiEventType::Unknown};
    }
    default:
      return {TuiEventType::Unknown};
  }
}
#endif

TuiEvent read_tui_event() {
#ifdef _WIN32
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  if (in != INVALID_HANDLE_VALUE) {
    while (true) {
      INPUT_RECORD record{};
      DWORD read = 0;
      if (!ReadConsoleInputW(in, &record, 1, &read) || read == 0) return {TuiEventType::Unknown};
      if (record.EventType == KEY_EVENT) {
        const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
        if (!key.bKeyDown) continue;
        wchar_t wc = key.uChar.UnicodeChar;
        if (wc == 8 || wc == 127) return {TuiEventType::Backspace};
        if (wc == 27) return read_windows_escape_event(in);
        if (wc == 3) return {TuiEventType::CtrlC};
        if (wc > 0 && wc < 27 && wc != 9 && wc != 13) return {TuiEventType::Character, static_cast<char>(wc)};
        if (wc == 21) return {TuiEventType::Character, static_cast<char>(21)};
        switch (key.wVirtualKeyCode) {
          case VK_UP: return {TuiEventType::Up};
          case VK_DOWN: return {TuiEventType::Down};
          case VK_LEFT: return {TuiEventType::Left};
          case VK_RIGHT: return {TuiEventType::Right};
          case VK_HOME: return {TuiEventType::Home};
          case VK_END: return {TuiEventType::End};
          case VK_DELETE: return {TuiEventType::DeleteKey};
          case VK_BACK: return {TuiEventType::Backspace};
          case VK_RETURN: return {TuiEventType::Enter};
          case VK_TAB: return {key.dwControlKeyState & SHIFT_PRESSED ? TuiEventType::BackTab : TuiEventType::Tab};
          case VK_ESCAPE: return {TuiEventType::Escape};
          default: break;
        }
        if (wc >= 32 && wc < 127) return {TuiEventType::Character, static_cast<char>(wc)};
      } else if (record.EventType == MOUSE_EVENT) {
        const MOUSE_EVENT_RECORD& mouse = record.Event.MouseEvent;
        if (mouse.dwEventFlags == 0 && (mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)) {
          int x = mouse.dwMousePosition.X + 1;
          int y = mouse.dwMousePosition.Y + 1;
          CONSOLE_SCREEN_BUFFER_INFO info{};
          HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
          if (out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(out, &info)) {
            x = mouse.dwMousePosition.X - info.srWindow.Left + 1;
            y = mouse.dwMousePosition.Y - info.srWindow.Top + 1;
          }
          return {TuiEventType::MousePress, 0, x, y};
        }
      }
    }
  }
#endif
  int ch = read_raw_char_blocking();
  if (ch < 0) return {TuiEventType::Escape};
  if (ch == 27) return read_escape_event();
  if (ch == 3) return {TuiEventType::CtrlC};
  if (ch == '\r' || ch == '\n') return {TuiEventType::Enter};
  if (ch == '\t') return {TuiEventType::Tab};
  if (ch == 8 || ch == 127) return {TuiEventType::Backspace};
  if (ch > 0 && ch < 27) return {TuiEventType::Character, static_cast<char>(ch)};
  if (ch == 21) return {TuiEventType::Character, static_cast<char>(21)};
  if (ch >= 32 && ch < 127) return {TuiEventType::Character, static_cast<char>(ch)};
  return {TuiEventType::Unknown};
}

#ifdef _WIN32
class TuiConsoleMode {
 public:
  TuiConsoleMode() {
    handle_ = GetStdHandle(STD_INPUT_HANDLE);
    active_ = handle_ != INVALID_HANDLE_VALUE && GetConsoleMode(handle_, &old_);
    if (active_) {
      DWORD mode = old_;
      mode |= ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT;
      mode &= ~ENABLE_QUICK_EDIT_MODE;
      SetConsoleMode(handle_, mode);
    }
  }
  ~TuiConsoleMode() {
    if (active_) SetConsoleMode(handle_, old_);
  }

 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  DWORD old_ = 0;
  bool active_ = false;
};
#else
class RawMode {
 public:
  RawMode() {
    active_ = tcgetattr(STDIN_FILENO, &old_) == 0;
    if (active_) {
      termios raw = old_;
      raw.c_lflag &= static_cast<unsigned>(~(ECHO | ICANON | ISIG | IEXTEN));
      raw.c_iflag &= static_cast<unsigned>(~(IXON | ICRNL));
      raw.c_cc[VMIN] = 1;
      raw.c_cc[VTIME] = 0;
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
  }
  ~RawMode() {
    if (active_) tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_);
  }
 private:
  bool active_ = false;
  termios old_{};
};
#endif

bool run_tui_command(const std::string& command, std::string& input, std::string& status, std::string& derive_var,
                     const overcalc::RenderOptions& opt) {
  if (command == ":q" || command == ":quit" || command == ":exit") return false;
  if (command == ":help") {
    std::cout << "\n";
    print_tui_help();
    status = "help printed below";
    return true;
  }
  if (command == ":symbols") {
    std::cout << "\n";
    print_supported_symbols(opt);
    status = "symbols printed below";
    return true;
  }
  if (command == ":clear") {
    input.clear();
    status = "cleared";
    return true;
  }
  if (command.rfind(":derive", 0) == 0) {
    std::string value = trim_copy(command.substr(7));
    if (value.empty() || value == "off") {
      derive_var.clear();
      status = "derive mode off";
    } else {
      derive_var = value;
      status = "derive mode d/d" + derive_var;
    }
    return true;
  }
  status = "unknown command; try :help";
  return true;
}

void move_cursor_from_mouse(int x, int y, std::string& input, std::size_t& cursor, std::string& status,
                            const overcalc::RenderOptions& opt) {
  if (y == kTuiInputRow) {
    int source_col = x - kTuiInputPrefixWidth - 1;
    cursor = static_cast<std::size_t>(std::clamp(source_col, 0, static_cast<int>(input.size())));
    status = "caret moved in source input";
    return;
  }

  if (y == kTuiSuggestionRow) {
    if (auto hit = suggestion_index_at_column(input, cursor, x)) {
      auto suggestions = completions_for(input, cursor);
      if (*hit < suggestions.size()) {
        apply_completion(input, cursor, suggestions[*hit]);
        status = "completed " + suggestions[*hit].label;
        return;
      }
    }
    status = "click a suggestion label to insert it";
    return;
  }

  if (y >= kTuiPreviewStartRow && !input.empty()) {
    try {
      auto equation = overcalc::render_equation_best_effort(input, opt);
      int equation_row = y - kTuiPrettyContentStartRow;
      int panel_width = std::clamp(opt.panel_width, 48, 140);
      int left_pad = std::max(0, (panel_width - 4 - equation.width) / 2);
      int equation_col = x - (3 + left_pad);
      if (equation_row >= 0 && equation_row < static_cast<int>(equation.lines.size())) {
        if (auto offset = overcalc::source_offset_at(equation, equation_row, equation_col)) {
          cursor = std::min(*offset, input.size());
          status = "visual equation caret moved";
          return;
        }
      }
    } catch (...) {
    }
    int width = std::max(1, opt.panel_width - 4);
    int col = std::clamp(x - 2, 0, width);
    cursor = static_cast<std::size_t>(
        std::clamp(static_cast<int>((static_cast<long long>(col) * static_cast<int>(input.size()) + width / 2) / width),
                   0, static_cast<int>(input.size())));
    status = "preview click mapped approximately";
    return;
  }

  status = "click input, suggestions, or the preview";
}

std::pair<int,int> editor_terminal_size() {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO info{};
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
    return {info.srWindow.Right-info.srWindow.Left+1, info.srWindow.Bottom-info.srWindow.Top+1};
#else
  winsize size{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size)==0 && size.ws_col && size.ws_row)
    return {size.ws_col,size.ws_row};
#endif
  return {80,24};
}

// All artwork uses single-column Unicode glyphs; byte offsets never become screen coordinates.
class EquationFrame {
 public:
  enum Style { Background, Ink, Accent, Muted, Subtle, Green, Lavender, Border, Active };
  struct Cell {char32_t glyph=U' ';Style style=Background;};
  EquationFrame(int w,int h,bool color):w_(w),h_(h),color_(color),cells_(h,std::vector<Cell>(w)) {}
  static std::u32string decode(const std::string& text) {
    std::u32string out;
    for(std::size_t i=0;i<text.size();) {
      unsigned char c=text[i++];char32_t cp=c;int extra=0;
      if(c>=0xf0) {cp=c&7;extra=3;} else if(c>=0xe0) {cp=c&15;extra=2;} else if(c>=0xc0) {cp=c&31;extra=1;}
      while(extra-- && i<text.size()) cp=(cp<<6)|(static_cast<unsigned char>(text[i++])&63);
      out+=cp;
    }
    return out;
  }
  void put(int x,int y,char32_t c,Style s) {if(x>=0 && x<w_ && y>=0 && y<h_) cells_[y][x]={c,s};}
  void tint(int x,int y,Style s) {if(x>=0 && x<w_ && y>=0 && y<h_) cells_[y][x].style=s;}
  void text(int x,int y,const std::u32string& text,Style s,int limit=10000) {
    int count=0;for(char32_t c:text) {if(count++>=limit) break;put(x++,y,c,s);}
  }
  void panel(int x,int y,int w,int h,const std::u32string& title,Style border=Border) {
    for(int col=1;col<w-1;++col) {put(x+col,y,U'─',border);put(x+col,y+h-1,U'─',border);}
    for(int row=1;row<h-1;++row) {put(x,y+row,U'│',border);put(x+w-1,y+row,U'│',border);}
    put(x,y,U'╭',border);put(x+w-1,y,U'╮',border);put(x,y+h-1,U'╰',border);put(x+w-1,y+h-1,U'╯',border);
    text(x+2,y,title,Muted,w-4);
  }
  std::vector<std::string> lines() const {
    // Explicit backgrounds keep transparent terminals readable without changing terminal settings.
    static const char* colors[]={"38;2;214;225;239;48;2;17;23;34","38;2;235;241;250;48;2;17;23;34",
      "38;2;112;211;222;48;2;17;23;34","38;2;151;168;190;48;2;17;23;34",
      "38;2;114;134;159;48;2;17;23;34","38;2;149;222;175;48;2;17;23;34",
      "38;2;195;178;244;48;2;17;23;34","38;2;63;82;108;48;2;17;23;34",
      "38;2;234;248;255;48;2;35;65;81"};
    std::vector<std::string> out;
    for(const auto& row:cells_) {
      std::string line;int previous=-1;
      for(const auto& cell:row) {
        if(color_ && previous!=cell.style) {line+="\x1b["+std::string(colors[cell.style])+"m";previous=cell.style;}
        line+=overcalc::editor_utf8({cell.glyph});
      }
      if(color_) line+="\x1b[0m";
      out.push_back(std::move(line));
    }
    return out;
  }
 private:
  int w_,h_;bool color_;std::vector<std::vector<Cell>> cells_;
};

overcalc::EditLayout template_preview(std::size_t index) {
  // Real editor layouts, with sample values, make the preview match the inserted structure.
  overcalc::ExpressionEditor sample;
  if(index==14) {sample.type('a');sample.insert(index);sample.type('b');}
  else if(index==15 || index==16) {sample.type('n');sample.insert(index);}
  else {
    sample.insert(index);
    const auto& t=overcalc::edit_templates()[index];
    for(int f=0;f<t.fields;++f) {
      char value='x';
      if(index==0) value=f==0?'a':'b';
      if(index==1) value=f==0?'x':'2';
      if(index==3) value=f==0?'a':'n';
      if(index==5) value=f==0?'3':'x';
      sample.type(value);sample.tab();
    }
  }
  return sample.layout();
}

int run_visual_editor(const overcalc::RenderOptions& opt) {
#ifdef _WIN32
  TuiConsoleMode raw;
#else
  RawMode raw;
#endif
  struct ScreenGuard {
    ScreenGuard() { std::cout << "\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h"; }
    ~ScreenGuard() { std::cout << "\x1b[?1000l\x1b[?1006l\x1b[0 q\x1b[?25h\x1b[0m\x1b[?1049l" << std::flush; }
  } guard;
  overcalc::ExpressionEditor editor;
  TuiScreen screen;
  bool palette = false;
  std::size_t selected = 0;
  int scroll_x = 0, scroll_y = 0;
  std::pair<int,int> previous_size{0,0};
  bool derive = false;
  std::string status = "Type to begin · / fraction · ^ superscript · Ctrl+R square root";
  while (true) {
    auto [cols, rows] = editor_terminal_size();
    if(previous_size!=std::make_pair(cols,rows)) { screen.invalidate(); previous_size={cols,rows}; }
    int width=std::max(1,cols-1);
    if(cols<40 || rows<18) {
      screen.draw({clip_visible("Resize to 40 × 18 or larger · Esc to exit",width)},std::make_pair(1,1));
      if(!input_available_now()) continue;
      auto event=read_tui_event();
      if(event.type==TuiEventType::Escape || event.type==TuiEventType::CtrlC) break;
      continue;
    }
    const auto& templates=overcalc::edit_templates();
    int panel_width=std::min(width-2,120), margin=(width-panel_width)/2;
    int card_count=std::max(2,panel_width/14);
    int card_width=panel_width/card_count;
    int page=static_cast<int>(selected)/card_count;
    int page_count=(static_cast<int>(templates.size())+card_count-1)/card_count;
    int palette_top=rows-8, result_top=palette_top-4;
    int canvas_top=4, canvas_height=std::max(1,result_top-canvas_top-1);
    int canvas_width=panel_width-6;
    auto layout=editor.layout(); auto caret=editor.caret();
    if(caret.x<scroll_x) scroll_x=caret.x;
    if(caret.x>=scroll_x+canvas_width) scroll_x=caret.x-canvas_width+1;
    if(caret.y<scroll_y) scroll_y=caret.y;
    if(caret.y>=scroll_y+canvas_height) scroll_y=caret.y-canvas_height+1;
    if(layout.width<=canvas_width) scroll_x=0;
    if(static_cast<int>(layout.lines.size())<=canvas_height) scroll_y=0;
    int equation_x=margin+3+std::max(0,(canvas_width-layout.width)/2);
    int equation_y=canvas_top+std::max(0,(canvas_height-static_cast<int>(layout.lines.size()))/2);
    EquationFrame frame(width,rows,opt.color);
    frame.text(margin,1,U"◈  OverCalc",EquationFrame::Accent);
    frame.text(margin+14,1,U"E Q U A T I O N   S T U D I O",EquationFrame::Muted);
    if(panel_width>75) frame.text(margin+panel_width-23,1,derive?U"d/dx  DERIVATIVE":U"●  LIVE EVALUATION",EquationFrame::Green);
    frame.panel(margin,3,panel_width,result_top-3,U" EQUATION ");
    for(int y=0;y<canvas_height;++y) {
      int ly=y+scroll_y;
      if(ly>=static_cast<int>(layout.lines.size())) break;
      for(int x=scroll_x;x<layout.width && x-scroll_x<canvas_width;++x) {
        char32_t ch=layout.lines[ly][x];
        auto style=EquationFrame::Ink;
        if(ch==U'□') style=EquationFrame::Muted;
        else if(ch==U'─' || ch==U'‾' || ch==U'√' || ch==U'│' || ch==U'×' || ch==U'+' || ch==U'−') style=EquationFrame::Accent;
        else if((ch>=U'a' && ch<=U'z') || (ch>=U'α' && ch<=U'ω')) style=EquationFrame::Lavender;
        frame.put(equation_x+x-scroll_x,equation_y+y,ch,style);
      }
    }
    // Tint the active field, keeping nested expression cells and caret coordinates intact.
    int field_begin=caret.x,field_end=caret.x;
    for(const auto& c:layout.carets) if(c.field==caret.field && c.y==caret.y) {field_begin=std::min(field_begin,c.x);field_end=std::max(field_end,c.x);}
    if(!palette) for(int x=field_begin;x<=field_end;++x) {
      int px=equation_x+x-scroll_x,py=equation_y+caret.y-scroll_y;
      if(px>=margin+3 && px<margin+panel_width-3 && py>=canvas_top && py<canvas_top+canvas_height)
        frame.tint(px,py,EquationFrame::Active);
    }
    frame.panel(margin,result_top,panel_width,3,derive?U" DERIVATIVE · d/dx ":U" RESULT ");
    std::string result_message="Complete the highlighted fields";
    auto result_style=EquationFrame::Muted;
    if(editor.complete()) {
      auto result=run_expression(editor.source(),1,false,derive?"x":"");
      if(result.ok) {
        result_message="=  "+result.exact;
        if(result.decimal && should_show_decimal(result.exact,*result.decimal)) {
          std::ostringstream decimal;decimal<<std::setprecision(10)<<static_cast<double>(*result.decimal);
          result_message+="    ≈  "+decimal.str();
        }
        result_style=EquationFrame::Green;
      } else {result_message="Editing · "+result.error;result_style=EquationFrame::Muted;}
    }
    frame.text(margin+3,result_top+1,frame.decode(result_message),result_style,panel_width-6);
    frame.text(margin,palette_top-1,palette?U"INSERT  ·  Enter to place":U"INSERT  ·  click an expression",EquationFrame::Muted,panel_width-23);
    std::string page_label="‹  "+std::to_string(page+1)+" / "+std::to_string(page_count)+"  ›";
    int page_x=margin+panel_width-15;
    frame.text(page_x,palette_top-1,frame.decode(page_label),EquationFrame::Accent);
    struct Button {int x,y,w,h;std::size_t index;};
    std::vector<Button> buttons;
    for(int slot=0;slot<card_count;++slot) {
      std::size_t index=page*card_count+slot;
      if(index>=templates.size()) break;
      int x=margin+slot*card_width,w=card_width-1;
      bool focused=palette && selected==index;
      frame.panel(x,palette_top,w,6,U"",focused?EquationFrame::Accent:EquationFrame::Border);
      auto preview=template_preview(index);
      int px=x+std::max(1,(w-preview.width)/2);
      int py=palette_top+1+std::max(0,(3-static_cast<int>(preview.lines.size()))/2);
      for(int r=0;r<std::min(3,static_cast<int>(preview.lines.size()));++r)
        frame.text(px,py+r,preview.lines[r],focused?EquationFrame::Accent:EquationFrame::Ink,w-2);
      auto label=frame.decode(templates[index].label);
      int lx=x+std::max(1,(w-static_cast<int>(label.size()))/2);
      frame.text(lx,palette_top+4,label,focused?EquationFrame::Accent:EquationFrame::Muted,w-2);
      buttons.push_back({x,palette_top,w,6,index});
    }
    frame.text(margin,rows-2,U"Tab fields   ↑↓←→ move   Ctrl+P insert   Ctrl+Z undo   Esc exit",EquationFrame::Muted,panel_width);
    frame.text(margin,rows-1,frame.decode(status),EquationFrame::Subtle,panel_width);
    int cx=equation_x+caret.x-scroll_x+1,cy=equation_y+caret.y-scroll_y+1;
    screen.draw(frame.lines(),palette?std::nullopt:std::optional{std::make_pair(cx,cy)});
    if(palette) std::cout<<"\x1b[?25l"<<std::flush;
    // Polling also redraws after a terminal resize, without needing a keypress.
    while (!input_available_now() && editor_terminal_size()==std::make_pair(cols,rows)) {}
    if (editor_terminal_size()!=std::make_pair(cols,rows)) continue;
    auto event=read_tui_event();
    if(event.type==TuiEventType::CtrlC) break;
    if(event.type==TuiEventType::Escape) { if(palette) { palette=false; continue; } break; }
    if(event.type==TuiEventType::Character && event.ch==16) { palette=!palette; continue; }
    if(event.type==TuiEventType::MousePress) {
      bool hit=false;
      if(event.y-1==palette_top-1 && event.x-1>=page_x && event.x-1<margin+panel_width) {
        int direction=event.x-1<page_x+6?-1:1;
        selected=((page+page_count+direction)%page_count)*card_count;palette=true;continue;
      }
      for(const auto& b:buttons) if(event.y-1>=b.y && event.y-1<b.y+b.h && event.x-1>=b.x && event.x-1<b.x+b.w) {
        editor.insert(b.index);selected=b.index;palette=false;status="Inserted "+templates[b.index].label+" · Tab moves to the next field";hit=true;break;
      }
      if(!hit && event.y-1>=canvas_top && event.y-1<canvas_top+canvas_height && event.x-1>margin && event.x-1<margin+panel_width-1) {
        editor.click(event.x-1-equation_x+scroll_x,event.y-1-equation_y+scroll_y);palette=false;
      }
      continue;
    }
    if(palette) {
      if(event.type==TuiEventType::Right || event.type==TuiEventType::Tab || event.type==TuiEventType::Down) selected=(selected+1)%templates.size();
      if(event.type==TuiEventType::Left || event.type==TuiEventType::BackTab || event.type==TuiEventType::Up) selected=(selected+templates.size()-1)%templates.size();
      if(event.type==TuiEventType::Enter) { editor.insert(selected); palette=false; status="Inserted "+templates[selected].label; }
      continue;
    }
    switch(event.type) {
      case TuiEventType::Left: editor.move(-1); break;
      case TuiEventType::Right: editor.move(1); break;
      case TuiEventType::Up: editor.vertical(-1); break;
      case TuiEventType::Down: editor.vertical(1); break;
      case TuiEventType::Tab: editor.tab(); break;
      case TuiEventType::BackTab: editor.tab(true); break;
      case TuiEventType::Home: editor.home(false); break;
      case TuiEventType::End: editor.home(true); break;
      case TuiEventType::Backspace: editor.erase(true); break;
      case TuiEventType::DeleteKey: editor.erase(false); break;
      case TuiEventType::Enter: status="Result updates as you type. Tab moves out of a field."; break;
      case TuiEventType::Character:
        if(event.ch==6) editor.insert(0);
        else if(event.ch==5) editor.insert(1);
        else if(event.ch==18) editor.insert(2);
        else if(event.ch==2) editor.insert(3);
        else if(event.ch==7) editor.insert(4);
        else if(event.ch==4) { derive=!derive; status=derive ? "Derivative mode enabled" : "Evaluation mode enabled"; }
        else if(event.ch==21) editor.clear();
        else if(event.ch==26) editor.undo();
        else if(event.ch==25) editor.undo(true);
        else editor.type(event.ch);
        break;
      default: break;
    }
  }
  return 0;
}

int run_tui(const overcalc::RenderOptions& opt) {
  std::string input;
  std::size_t cursor = 0;
  std::string status = "Tab completes; click input/suggestions/preview; :help lists commands";
  std::string derive_var;

  if (!stdin_is_terminal()) {
    std::string line;
    while (std::getline(std::cin, line)) {
      line = trim_copy(line);
      if (line.empty()) continue;
      if (line[0] == ':') {
        if (!run_tui_command(line, input, status, derive_var, opt)) break;
        continue;
      }
      input = line;
      std::cout << tui_preview(input, derive_var, opt);
    }
    return 0;
  }

  return run_visual_editor(opt);
}

bool is_plain_integer(const std::string& text) {
  if (text.empty()) return false;
  size_t start = text[0] == '-' ? 1 : 0;
  if (start == text.size()) return false;
  for (size_t i = start; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
  }
  return true;
}

bool is_exact_numeric(const std::string& text) {
  if (is_plain_integer(text)) return true;
  auto slash = text.find('/');
  if (slash == std::string::npos) return false;
  return is_plain_integer(text.substr(0, slash)) && is_plain_integer(text.substr(slash + 1));
}

bool should_show_decimal(const std::string& exact, long double decimal) {
  if (!is_plain_integer(exact)) return true;
  try {
    long double exact_value = std::stold(exact);
    return std::fabs(exact_value - decimal) > 1e-12L;
  } catch (...) {
    return true;
  }
}

void configure_console() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {
      SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
  }
#endif
}

int terminal_width() {
#ifdef _WIN32
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out != INVALID_HANDLE_VALUE) {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(out, &info)) {
      return info.srWindow.Right - info.srWindow.Left + 1;
    }
  }
#else
  if (const char* columns = std::getenv("COLUMNS")) {
    try {
      return std::stoi(columns);
    } catch (...) {
    }
  }
#endif
  return 88;
}

}  // namespace

int main(int argc, char** argv) {
  configure_console();

  if (argc < 2) {
    std::cerr << usage_text();
    return 2;
  }

  overcalc::RenderOptions ropt;
  ropt.panel_width = std::clamp(terminal_width() - 2, 68, 120);
  bool latex_output = false;
  bool json_output = false;
  bool ast_json = false;
  bool timing_on = false;
  bool batch_on = false;
  bool tui_on = false;
  bool steps_on = false;
  bool read_stdin_input = false;
  bool list_symbols = false;
  std::string derive_var;
  std::string file_input;
  std::string expr;
  std::vector<std::string> expr_parts;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--") {
      while (++i < argc) expr_parts.emplace_back(argv[i]);
      break;
    }
    if (arg == "--help" || arg == "-h") {
      std::cout << usage_text();
      return 0;
    }
    else if (arg == "--version") {
      std::cout << "OverCalc " << kVersion << "\n";
      return 0;
    }
    else if (arg == "--ascii") ropt.ascii = true;
    else if (arg == "--unicode") ropt.ascii = false;
    else if (arg == "--no-color") ropt.color = false;
    else if (arg == "--width") {
      if (i + 1 >= argc) {
        std::cerr << "error: --width requires a number\n";
        return 2;
      }
      try {
        ropt.panel_width = std::stoi(argv[++i]);
      } catch (...) {
        std::cerr << "error: invalid --width value\n";
        return 2;
      }
    }
    else if (arg == "--latex-output") latex_output = true;
    else if (arg == "--json") json_output = true;
    else if (arg == "--ast-json") ast_json = true;
    else if (arg == "--timing") timing_on = true;
    else if (arg == "--batch") batch_on = true;
    else if (arg == "--tui") tui_on = true;
    else if (arg == "--derive" || arg == "--diff" || arg == "--derivative") {
      if (i + 1 >= argc) {
        std::cerr << "error: " << arg << " requires a variable\n";
        return 2;
      }
      derive_var = argv[++i];
    }
    else if (arg == "--steps") steps_on = true;
    else if (arg == "--stdin" || arg == "-") read_stdin_input = true;
    else if (arg == "--list-symbols") list_symbols = true;
    else if (arg == "--file") {
      if (i + 1 >= argc) {
        std::cerr << "error: --file requires a path\n";
        return 2;
      }
      file_input = argv[++i];
    }
    else if (arg.rfind("--", 0) == 0) {
      std::cerr << "error: unknown option: " << arg << "\n";
      return 2;
    }
    else expr_parts.push_back(arg);
  }

  if (list_symbols) {
    print_supported_symbols(ropt);
    return 0;
  }

  if (tui_on) {
    return run_tui(ropt);
  }

  int input_sources = (!expr_parts.empty() ? 1 : 0) + (read_stdin_input ? 1 : 0) + (!file_input.empty() ? 1 : 0);
  if (input_sources > 1) {
    std::cerr << "error: choose only one input source (expression, --stdin, or --file)\n";
    return 2;
  }

  try {
    if (read_stdin_input) expr = read_stdin();
    else if (!file_input.empty()) expr = read_file(file_input);
    else expr = join_args(expr_parts);
  } catch (const std::exception& err) {
    std::cerr << "error: " << err.what() << "\n";
    return 1;
  }

  if (expr.empty()) {
    std::cerr << "error: missing expression\n";
    return 2;
  }

  if (batch_on) {
    if (ast_json) {
      std::cerr << "error: --batch cannot be combined with --ast-json\n";
      return 2;
    }
    if (latex_output) {
      std::cerr << "error: --batch cannot be combined with --latex-output\n";
      return 2;
    }
    return print_batch(expr, json_output, steps_on, timing_on, derive_var);
  }

  try {
    auto start = std::chrono::steady_clock::now();
    auto ast = overcalc::parse(expr);
    if (ast_json) {
      std::cout << overcalc::ast_to_json(*ast) << "\n";
      if (timing_on) print_timing(elapsed_ms(start));
      return 0;
    }

    auto simplified = overcalc::simplify(*ast);
    std::string original_text = overcalc::to_infix(*ast);
    std::string derivative_text;
    std::string derivative_latex;
    if (!derive_var.empty()) {
      auto derivative_expr = overcalc::simplify(*overcalc::derivative(*simplified, derive_var));
      derivative_text = overcalc::to_infix(*derivative_expr);
      derivative_latex = overcalc::to_latex(*derivative_expr);
      simplified = std::move(derivative_expr);
    }
    std::string simplified_text = overcalc::to_infix(*simplified);
    overcalc::StepLog logs;
    auto result_value = overcalc::evaluate(*simplified, (steps_on || json_output) ? &logs : nullptr);
    if ((steps_on || json_output) && !derive_var.empty()) {
      logs.entries.insert(logs.entries.begin() + std::min<std::size_t>(1, logs.entries.size()),
                          "Derivative d/d" + derive_var + ": " + derivative_text);
    }
    if ((steps_on || json_output) && derive_var.empty() && simplified_text != original_text) {
      logs.entries.insert(logs.entries.begin() + std::min<std::size_t>(1, logs.entries.size()),
                          "Simplified: " + simplified_text);
    }
    double total_ms = elapsed_ms(start);

    if (json_output) {
      print_result_json(expr, *ast, *simplified, result_value, logs, steps_on,
                        timing_on ? std::optional<double>{total_ms} : std::nullopt,
                        derivative_text, derivative_latex);
      return 0;
    }

    if (latex_output) {
      std::cout << overcalc::to_latex(*ast);
      if (!derive_var.empty()) std::cout << " \\xrightarrow{d/d" << derive_var << "} " << derivative_latex;
      std::cout << " = " << result_value.exact;
      if (result_value.decimal && should_show_decimal(result_value.exact, *result_value.decimal)) {
        std::cout << " \\approx " << std::setprecision(12) << static_cast<double>(*result_value.decimal);
      }
      std::cout << "\n";
      if (steps_on) {
        std::cout << "\n-- Steps --\n";
        for (const auto& entry : logs.entries) std::cout << "* " << entry << "\n";
      }
      if (timing_on) print_timing(total_ms);
      return 0;
    }

    std::ostringstream result_out;
    result_out << (is_exact_numeric(result_value.exact) ? "Exact    " : "Form     ") << result_value.exact;
    if (result_value.decimal && should_show_decimal(result_value.exact, *result_value.decimal)) {
      result_out << "\nDecimal  " << std::setprecision(12) << static_cast<double>(*result_value.decimal);
    }

    std::string extra_panel = simplified_text != original_text ? simplified_text : "";
    std::string extra_title = !derive_var.empty() ? "Derivative d/d" + derive_var : "Simplified";
    std::cout << overcalc::render_boxed(expr, result_out.str(), ropt, extra_panel, extra_title);
    if (steps_on) std::cout << overcalc::render_steps_boxed(logs.entries, ropt);
    if (timing_on) print_timing(total_ms);
    return 0;
  } catch (const overcalc::ParseError& err) {
    std::cerr << stderr_ansi("parse error", "31;1", ropt) << " at column " << (err.offset() + 1)
              << ": " << err.what() << "\n";
    std::cerr << expr << "\n" << std::string(err.offset(), ' ')
              << stderr_ansi("^", "36;1", ropt) << "\n";
    return 1;
  } catch (const std::exception& err) {
    std::cerr << stderr_ansi("error", "31;1", ropt) << ": " << err.what() << "\n";
    return 1;
  }
}
