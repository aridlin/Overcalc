#include "overcalc/render.hpp"

#include "overcalc/ast.hpp"
#include "overcalc/parser.hpp"
#include "overcalc/symbols.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace overcalc {
namespace {

struct Block {
  std::vector<std::string> lines;
  int width = 0;
  int baseline = 0;
};

struct Glyphs {
  std::string h, tl, tr, bl, br, v, title;
  std::string sqrt, frac;
};

enum class Align { Left, Center };

static int display_width(const std::string& s) {
  int width = 0;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
      i += 2;
      while (i < s.size() && s[i] != 'm') ++i;
      if (i < s.size()) ++i;
      continue;
    }
    if ((c & 0xC0) != 0x80) ++width;
    ++i;
  }
  return width;
}

static size_t utf8_len(unsigned char c) {
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

static std::string clip_width(const std::string& s, int max_width) {
  std::string out;
  int width = 0;
  for (size_t i = 0; i < s.size() && width < max_width;) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t len = utf8_len(c);
    if (i + len > s.size()) break;
    out.append(s, i, len);
    i += len;
    ++width;
  }
  return out;
}

static std::string sp(int n) { return std::string(std::max(0, n), ' '); }

static std::string rep(const std::string& g, int n) {
  std::string out;
  for (int i = 0; i < std::max(0, n); ++i) out += g;
  return out;
}

static Block txt(std::string s) {
  int width = display_width(s);
  return Block{{std::move(s)}, width, 0};
}

static std::vector<std::string> split_lines(const std::string& text) {
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
  lines.push_back(line);
  return lines;
}

static Block large_symbol(std::string name) {
  if (name == "sum") return Block{{"\xE2\x95\xB2\xE2\x94\x80\xE2\x94\x80", " > ", "\xE2\x95\xB1\xE2\x94\x80\xE2\x94\x80"}, 3, 1};
  if (name == "prod") return Block{{"\xE2\x94\xAC \xE2\x94\xAC", "\xE2\x94\x82 \xE2\x94\x82", "\xE2\x94\xB4 \xE2\x94\xB4"}, 3, 1};
  if (name == "int") return Block{{"\xE2\x8C\xA0", "\xE2\x8E\xAE", "\xE2\x8C\xA1"}, 1, 1};
  return txt(std::move(name));
}

static Glyphs glyphs_for(const RenderOptions& opt) {
  if (opt.ascii) return {"-", "+", "+", "+", "+", "|", "-", "sqrt", "-"};
  return {"\xE2\x94\x80", "\xE2\x95\xAD", "\xE2\x95\xAE", "\xE2\x95\xB0", "\xE2\x95\xAF",
          "\xE2\x94\x82", "\xE2\x94\x80", "\xE2\x88\x9A", "\xE2\x94\x80"};
}

static std::string ansi(const std::string& text, const std::string& code, const RenderOptions& opt) {
  return opt.color ? "\x1b[" + code + "m" + text + "\x1b[0m" : text;
}

static bool is_function_name(const std::string& word) {
  return word == "sin" || word == "cos" || word == "tan" || word == "ln" || word == "log" ||
         word == "exp" || word == "abs" || word == "sqrt";
}

static bool is_operator_glyph(const std::string& token) {
  return token == "+" || token == "-" || token == "*" || token == "/" || token == "=" ||
         token == "^" || token == "!" || token == "%" || token == ">" ||
         token == "\xE2\x94\x80" || token == "\xE2\x88\x9A" || token == "\xE2\x95\xB2" ||
         token == "\xE2\x95\xB1" || token == "\xE2\x8C\xA0" || token == "\xE2\x8E\xAE" ||
         token == "\xE2\x8C\xA1" || token == "\xE2\x94\xAC" || token == "\xE2\x94\xB4" ||
         token == "\xE2\x94\x82" || token == "\xC2\xB7";
}

static bool is_delimiter_glyph(const std::string& token) {
  return token == "(" || token == ")" || token == "[" || token == "]" || token == "{" ||
         token == "}" || token == "|" || token == "\xE2\x8E\x9B" || token == "\xE2\x8E\x9C" ||
         token == "\xE2\x8E\x9D" || token == "\xE2\x8E\x9E" || token == "\xE2\x8E\x9F" ||
         token == "\xE2\x8E\xA0";
}

static std::string highlight_formula(const std::string& line, const RenderOptions& opt) {
  if (!opt.color) return line;

  std::string out;
  for (size_t i = 0; i < line.size();) {
    unsigned char c = static_cast<unsigned char>(line[i]);
    if (std::isspace(c)) {
      out.push_back(line[i++]);
      continue;
    }

    if (std::isdigit(c) || (line[i] == '.' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
      size_t start = i;
      while (i < line.size() && (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.')) ++i;
      out += ansi(line.substr(start, i - start), "32;1", opt);
      continue;
    }

    if (line[i] == '\\') {
      size_t start = i++;
      while (i < line.size() && std::isalpha(static_cast<unsigned char>(line[i]))) ++i;
      out += ansi(line.substr(start, i - start), "36;1", opt);
      continue;
    }

    if (std::isalpha(c)) {
      size_t start = i;
      while (i < line.size() && std::isalnum(static_cast<unsigned char>(line[i]))) ++i;
      std::string word = line.substr(start, i - start);
      out += ansi(word, is_function_name(word) ? "36;1" : "35;1", opt);
      continue;
    }

    size_t len = utf8_len(c);
    if (i + len > line.size()) len = 1;
    std::string token = line.substr(i, len);
    if (is_operator_glyph(token)) out += ansi(token, "33;1", opt);
    else if (is_delimiter_glyph(token)) out += ansi(token, "34;1", opt);
    else out += token;
    i += len;
  }
  return out;
}

static Block pad(const Block& b, int top, int bot, int w) {
  Block out;
  out.width = w;
  out.baseline = b.baseline + top;
  for (int i = 0; i < top; ++i) out.lines.push_back(sp(w));
  for (auto line : b.lines) {
    line += sp(w - display_width(line));
    out.lines.push_back(std::move(line));
  }
  for (int i = 0; i < bot; ++i) out.lines.push_back(sp(w));
  return out;
}

static Block hcat(const Block& a, const Block& b, int gap = 1) {
  int top = std::max(a.baseline, b.baseline);
  int a_bottom = static_cast<int>(a.lines.size()) - a.baseline - 1;
  int b_bottom = static_cast<int>(b.lines.size()) - b.baseline - 1;
  int bottom = std::max(a_bottom, b_bottom);
  Block left = pad(a, top - a.baseline, bottom - a_bottom, a.width);
  Block right = pad(b, top - b.baseline, bottom - b_bottom, b.width);
  Block out;
  out.width = left.width + gap + right.width;
  out.baseline = top;
  out.lines.resize(left.lines.size());
  for (size_t i = 0; i < out.lines.size(); ++i) out.lines[i] = left.lines[i] + sp(gap) + right.lines[i];
  return out;
}

static std::string left_delim(int row, int height, bool ascii) {
  if (ascii || height <= 1) return "(";
  if (row == 0) return "\xE2\x8E\x9B";
  if (row == height - 1) return "\xE2\x8E\x9D";
  return "\xE2\x8E\x9C";
}

static std::string right_delim(int row, int height, bool ascii) {
  if (ascii || height <= 1) return ")";
  if (row == 0) return "\xE2\x8E\x9E";
  if (row == height - 1) return "\xE2\x8E\xA0";
  return "\xE2\x8E\x9F";
}

static Block paren(const Block& b, bool ascii) {
  int height = static_cast<int>(b.lines.size());
  if (height <= 1) return hcat(hcat(txt("("), b, 0), txt(")"), 0);

  Block out;
  out.width = b.width + 2;
  out.baseline = b.baseline;
  out.lines.reserve(b.lines.size());
  for (int row = 0; row < height; ++row) {
    auto line = b.lines[row];
    line += sp(b.width - display_width(line));
    out.lines.push_back(left_delim(row, height, ascii) + line + right_delim(row, height, ascii));
  }
  return out;
}

static Block paren(const Block& b) {
  return hcat(hcat(txt("("), b, 0), txt(")"), 0);
}

static Block vfrac(const Block& n, const Block& d, const Glyphs& g) {
  int w = std::max(n.width, d.width) + 2;
  auto center = [&](const Block& b) {
    std::vector<std::string> out;
    int left = (w - b.width) / 2;
    for (auto line : b.lines) out.push_back(sp(left) + line + sp(w - left - display_width(line)));
    return out;
  };

  Block out;
  auto top = center(n);
  auto bottom = center(d);
  out.lines.insert(out.lines.end(), top.begin(), top.end());
  out.lines.push_back(rep(g.frac, w));
  out.lines.insert(out.lines.end(), bottom.begin(), bottom.end());
  out.width = w;
  out.baseline = static_cast<int>(top.size());
  return out;
}

static Block vsqrt(const Block& r, const Glyphs& g) {
  if (g.sqrt != "sqrt") {
    Block out;
    out.width = r.width + 2;
    out.baseline = r.baseline + 1;
    out.lines.push_back(sp(2) + rep(g.frac, r.width));
    for (size_t i = 0; i < r.lines.size(); ++i) {
      auto line = r.lines[i] + sp(r.width - display_width(r.lines[i]));
      if (i == 0) out.lines.push_back(g.sqrt + std::string(" ") + line);
      else out.lines.push_back(std::string(2, ' ') + line);
    }
    return out;
  }

  Block out;
  out.width = r.width + 2;
  out.baseline = r.baseline;
  for (size_t i = 0; i < r.lines.size(); ++i) {
    if (i == 0) out.lines.push_back(g.sqrt + std::string(" ") + r.lines[i]);
    else out.lines.push_back(std::string(2, ' ') + r.lines[i]);
  }
  return out;
}

static Block vroot(const Block& r, const Block* index, const Glyphs& g, bool ascii) {
  if (!index) return vsqrt(r, g);
  if (ascii) return hcat(hcat(hcat(txt("root["), *index, 0), txt("]"), 0), r);
  return hcat(*index, vsqrt(r, g), 0);
}

static Block superscript(const Block& base, const Block& exponent, bool ascii) {
  if (ascii) return hcat(hcat(base, txt("^"), 0), exponent, 0);

  Block out;
  out.width = base.width + exponent.width;
  out.baseline = static_cast<int>(exponent.lines.size()) + base.baseline;
  int height = static_cast<int>(exponent.lines.size() + base.lines.size());
  out.lines.reserve(height);
  for (int row = 0; row < height; ++row) {
    std::string left = row >= static_cast<int>(exponent.lines.size())
                           ? base.lines[row - static_cast<int>(exponent.lines.size())]
                           : sp(base.width);
    std::string right = row < static_cast<int>(exponent.lines.size()) ? exponent.lines[row] : sp(exponent.width);
    left += sp(base.width - display_width(left));
    right += sp(exponent.width - display_width(right));
    out.lines.push_back(left + right);
  }
  return out;
}

static Block subscript_block(const Block& base, const Block& sub, bool ascii) {
  if (ascii) return hcat(hcat(base, txt("_"), 0), sub, 0);

  Block out;
  out.width = base.width + sub.width;
  out.baseline = base.baseline;
  int height = static_cast<int>(base.lines.size() + sub.lines.size());
  out.lines.reserve(height);
  for (int row = 0; row < height; ++row) {
    std::string left = row < static_cast<int>(base.lines.size()) ? base.lines[row] : sp(base.width);
    std::string right = row >= static_cast<int>(base.lines.size()) ? sub.lines[row - base.lines.size()] : sp(sub.width);
    left += sp(base.width - display_width(left));
    right += sp(sub.width - display_width(right));
    out.lines.push_back(left + right);
  }
  return out;
}

static int precedence(const Expr& e) {
  if (auto b = dynamic_cast<const Binary*>(&e)) return (b->op == '+' || b->op == '-') ? 1 : 2;
  return dynamic_cast<const Superscript*>(&e) ? 3 : 4;
}

static bool needs_paren(const Expr& e, int parent_prec, bool right_child, char parent_op) {
  int child_prec = precedence(e);
  return child_prec < parent_prec ||
         (right_child && child_prec == parent_prec && (parent_op == '-' || parent_op == '/'));
}

static bool is_large_operator_family(const Expr& e) {
  if (auto s = dynamic_cast<const Symbol*>(&e)) return s->name == "sum" || s->name == "prod" || s->name == "int";
  if (auto sub = dynamic_cast<const Subscript*>(&e)) return is_large_operator_family(*sub->base);
  if (auto sup = dynamic_cast<const Superscript*>(&e)) return is_large_operator_family(*sup->base);
  return false;
}

static bool needs_script_grouping(const Expr& e, const Block& rendered, char parent_op) {
  if (is_large_operator_family(e)) return false;
  return needs_paren(e, 3, false, parent_op) || dynamic_cast<const Fraction*>(&e) ||
         dynamic_cast<const Sqrt*>(&e) || rendered.lines.size() > 1;
}

static Block render_expr_prec(const Expr& e, const Glyphs& g, bool ascii, int parent_prec, bool right_child,
                              char parent_op) {
  Block out;
  if (auto n = dynamic_cast<const Number*>(&e)) return txt(std::to_string(n->value));
  if (auto d = dynamic_cast<const Decimal*>(&e)) return txt(d->text);
  if (auto s = dynamic_cast<const Symbol*>(&e)) return ascii ? txt(render_symbol(s->name, true)) : large_symbol(s->name);

  if (auto fn = dynamic_cast<const FunctionCall*>(&e)) {
    auto arg = render_expr_prec(*fn->arg, g, ascii, 0, false, 0);
    if (fn->name == "fact") out = hcat(arg, txt("!"), 0);
    else if (fn->name == "percent") out = hcat(arg, txt("%"), 0);
    else if (fn->name == "abs") out = hcat(hcat(txt("|"), arg, 0), txt("|"), 0);
    else out = hcat(txt(fn->name), paren(arg, ascii), 0);
  } else if (auto f = dynamic_cast<const Fraction*>(&e)) {
    out = vfrac(render_expr_prec(*f->num, g, ascii, 0, false, 0),
                render_expr_prec(*f->den, g, ascii, 0, false, 0), g);
  } else if (auto q = dynamic_cast<const Sqrt*>(&e)) {
    auto radicand = render_expr_prec(*q->radicand, g, ascii, 0, false, 0);
    if (q->index) {
      auto index = render_expr_prec(*q->index, g, ascii, 0, false, 0);
      out = vroot(radicand, &index, g, ascii);
    } else {
      out = vroot(radicand, nullptr, g, ascii);
    }
  } else if (auto p = dynamic_cast<const Superscript*>(&e)) {
    auto base = render_expr_prec(*p->base, g, ascii, 0, false, 0);
    auto exponent = render_expr_prec(*p->exponent, g, ascii, 0, false, 0);
    if (needs_script_grouping(*p->base, base, '^')) base = paren(base, ascii);
    out = superscript(base, exponent, ascii);
  } else if (auto sub = dynamic_cast<const Subscript*>(&e)) {
    auto base = render_expr_prec(*sub->base, g, ascii, 0, false, 0);
    auto subscript = render_expr_prec(*sub->sub, g, ascii, 0, false, 0);
    if (needs_script_grouping(*sub->base, base, '_')) base = paren(base, ascii);
    out = subscript_block(base, subscript, ascii);
  } else if (auto b = dynamic_cast<const Binary*>(&e)) {
    if (b->op == '*') {
      if (auto n = dynamic_cast<const Number*>(b->lhs.get()); n && n->value == -1) {
        out = hcat(txt("-"), render_expr_prec(*b->rhs, g, ascii, 3, false, '-'), 0);
        return needs_paren(e, parent_prec, right_child, parent_op) ? paren(out, ascii) : out;
      }
    }
    int prec = precedence(e);
    if (b->op == '/') {
      out = vfrac(render_expr_prec(*b->lhs, g, ascii, 0, false, 0),
                  render_expr_prec(*b->rhs, g, ascii, 0, false, 0), g);
    } else {
      std::string op = b->op == '*' && !ascii ? "\xC2\xB7" : std::string(1, b->op);
      out = hcat(hcat(render_expr_prec(*b->lhs, g, ascii, prec, false, b->op), txt(op)),
                 render_expr_prec(*b->rhs, g, ascii, prec, true, b->op));
    }
  } else {
    out = txt("?");
  }

  return needs_paren(e, parent_prec, right_child, parent_op) ? paren(out, ascii) : out;
}

static Block render_expr(const Expr& e, const Glyphs& g, bool ascii) {
  return render_expr_prec(e, g, ascii, 0, false, 0);
}

static std::string panel(const std::string& title, const std::vector<std::string>& content, int width,
                         const Glyphs& g, const RenderOptions& opt, const std::string& accent,
                         const std::string& content_color = "", Align align = Align::Left,
                         bool syntax_highlight = false) {
  std::ostringstream out;
  std::string top = g.tl + g.title + " " + title + " ";
  if (display_width(top) < width - 1) top += rep(g.h, width - 1 - display_width(top));
  out << ansi(top + g.tr, accent, opt) << "\n";

  for (auto line : content) {
    if (display_width(line) > width - 4) line = clip_width(line, width - 4);
    int remaining = width - 4 - display_width(line);
    int left_pad = align == Align::Center ? remaining / 2 : 0;
    int right_pad = remaining - left_pad;
    std::string rendered = syntax_highlight ? highlight_formula(line, opt)
                                            : (content_color.empty() ? line : ansi(line, content_color, opt));
    out << ansi(g.v, accent, opt) << " "
        << sp(left_pad) << rendered
        << sp(right_pad) << " " << ansi(g.v, accent, opt) << "\n";
  }

  out << ansi(g.bl + rep(g.h, width - 2) + g.br, accent, opt) << "\n";
  return out.str();
}

}  // namespace

std::string render_boxed(const std::string& input, const std::string& result, const RenderOptions& options,
                         const std::string& extra, const std::string& extra_title) {
  auto g = glyphs_for(options);
  std::vector<std::string> pretty;
  try {
    auto ast = parse(input);
    pretty = render_expr(*ast, g, options.ascii).lines;
  } catch (...) {
    pretty = {input};
  }
  std::vector<std::string> simplified_pretty;
  if (!extra.empty()) {
    try {
      auto ast = parse(extra);
      simplified_pretty = render_expr(*ast, g, options.ascii).lines;
    } catch (...) {
      simplified_pretty = {extra};
    }
  }

  int width = std::clamp(options.panel_width, 48, 140);
  std::ostringstream out;
  std::string mode = options.ascii ? "ASCII" : "Unicode";
  std::string color = options.color ? "color" : "plain";
  std::string bullet = options.ascii ? " -" : " \xE2\x80\xA2";
  out << ansi("OverCalc", "36;1", options)
      << ansi(bullet + " " + mode + " math" + bullet + " " + color + " output", "90", options)
      << "\n";
  out << panel("OverCalc / Input", {input}, width, g, options, "36", "", Align::Left, true);
  out << panel("Pretty Render", pretty, width, g, options, "35", "", Align::Center, true);
  if (!simplified_pretty.empty()) {
    out << panel(extra_title, simplified_pretty, width, g, options, "34", "", Align::Center, true);
  }
  out << panel("Result", split_lines(result), width, g, options, "32", "32;1");
  return out.str();
}

std::string render_steps_boxed(const std::vector<std::string>& steps, const RenderOptions& options) {
  if (steps.empty()) return "";

  auto g = glyphs_for(options);
  std::vector<std::string> lines;
  lines.reserve(steps.size());
  std::string bullet = options.ascii ? "* " : "\xE2\x80\xBA ";
  for (const auto& step : steps) lines.push_back(bullet + step);

  std::ostringstream out;
  out << "\n" << panel("Steps", lines, std::clamp(options.panel_width, 48, 140), g, options, "33", "37");
  return out.str();
}

}  // namespace overcalc
