#include "overcalc/ast_dump.hpp"
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
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

constexpr const char* kVersion = "0.1.0";

bool should_show_decimal(const std::string& exact, long double decimal);

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
