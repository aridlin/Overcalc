#include "overcalc/render.hpp"

#include "overcalc/ast.hpp"
#include "overcalc/parser.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace overcalc {
namespace {

struct Block { std::vector<std::string> lines; int width = 0; int baseline = 0; };

static std::string repeat_space(int n) { return std::string(std::max(0, n), ' '); }
static std::string repeat_utf8(const std::string& glyph, int n) { std::string out; for (int i=0;i<std::max(0,n);++i) out += glyph; return out; }

static Block text_block(std::string s) { return Block{{std::move(s)}, static_cast<int>(s.size()), 0}; }

static Block pad_height(const Block& b, int top, int bottom, int total_w) {
  Block out; out.width = total_w; out.baseline = b.baseline + top;
  for (int i = 0; i < top; ++i) out.lines.push_back(repeat_space(total_w));
  for (auto line : b.lines) { line += repeat_space(total_w - static_cast<int>(line.size())); out.lines.push_back(std::move(line)); }
  for (int i = 0; i < bottom; ++i) out.lines.push_back(repeat_space(total_w));
  return out;
}

static Block hcat(const Block& a, const Block& b, int gap = 1) {
  int top = std::max(a.baseline, b.baseline);
  int a_bottom = static_cast<int>(a.lines.size()) - a.baseline - 1;
  int b_bottom = static_cast<int>(b.lines.size()) - b.baseline - 1;
  int bottom = std::max(a_bottom, b_bottom);
  Block aa = pad_height(a, top - a.baseline, bottom - a_bottom, a.width);
  Block bb = pad_height(b, top - b.baseline, bottom - b_bottom, b.width);
  Block out; out.width = aa.width + gap + bb.width; out.baseline = top; out.lines.resize(aa.lines.size());
  for (size_t i = 0; i < out.lines.size(); ++i) out.lines[i] = aa.lines[i] + repeat_space(gap) + bb.lines[i];
  return out;
}

static Block vfrac(const Block& n, const Block& d) {
  int w = std::max(n.width, d.width) + 2;
  auto center = [&](const Block& b) { std::vector<std::string> out; int left = (w - b.width) / 2; for (auto line : b.lines) out.push_back(repeat_space(left) + line + repeat_space(w - left - static_cast<int>(line.size()))); return out; };
  Block out; auto top = center(n); auto bot = center(d);
  out.lines.insert(out.lines.end(), top.begin(), top.end());
  out.lines.push_back(repeat_utf8("─", w));
  out.lines.insert(out.lines.end(), bot.begin(), bot.end());
  out.width = w; out.baseline = static_cast<int>(top.size()); return out;
}

static Block render_expr(const Expr& e) {
  if (auto n = dynamic_cast<const Number*>(&e)) return text_block(std::to_string(n->value));
  if (auto f = dynamic_cast<const Fraction*>(&e)) return vfrac(render_expr(*f->num), render_expr(*f->den));
  auto b = dynamic_cast<const Binary*>(&e); if (!b) return text_block("?");
  return hcat(hcat(render_expr(*b->lhs), text_block(std::string(1, b->op)), 1), render_expr(*b->rhs), 1);
}

static std::string panel(const std::string& title, const std::vector<std::string>& content, int width) {
  std::ostringstream o;
  std::string top = "╭─ " + title + " ";
  if (static_cast<int>(top.size()) < width - 1) top += repeat_utf8("─", width - 1 - static_cast<int>(top.size()));
  o << top << "╮\n";
  for (auto line : content) {
    if (static_cast<int>(line.size()) > width - 4) line = line.substr(0, width - 4);
    o << "│ " << line << repeat_space(width - 4 - static_cast<int>(line.size())) << " │\n";
  }
  o << "╰" << repeat_utf8("─", width - 2) << "╯\n";
  return o.str();
}

}  // namespace

std::string render_boxed(const std::string& input, const std::string& result) {
  std::vector<std::string> pretty_lines;
  try { auto ast = parse(input); pretty_lines = render_expr(*ast).lines; } catch (...) { pretty_lines = {input}; }
  int width = 64;
  std::ostringstream o;
  o << panel("OverCalc / Input", {input}, width);
  o << panel("Pretty Render", pretty_lines, width);
  o << panel("Result", {result}, width);
  return o.str();
}

}  // namespace overcalc
