#include "overcalc/render.hpp"

#include "overcalc/ast.hpp"
#include "overcalc/parser.hpp"
#include "overcalc/symbols.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace overcalc {
namespace {

struct Block {
  std::vector<std::string> lines;
  int width = 0;
  int baseline = 0;
  std::vector<SourceHit> hits;
  std::size_t edit_start = npos;
  std::size_t edit_end = npos;
  int edit_depth = 0;
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

static void insert_at_display_column(std::string& line, int column, const std::string& marker) {
  int width = 0;
  for (size_t i = 0; i < line.size();) {
    if (width >= column) {
      line.insert(i, marker);
      return;
    }
    unsigned char c = static_cast<unsigned char>(line[i]);
    size_t len = utf8_len(c);
    if (i + len > line.size()) len = 1;
    i += len;
    ++width;
  }
  if (width < column) line += std::string(column - width, ' ');
  line += marker;
}

static bool style_display_column(std::string& line, int column, const std::string& code, const RenderOptions& opt) {
  if (!opt.color) return true;
  int width = 0;
  for (size_t i = 0; i < line.size();) {
    unsigned char c = static_cast<unsigned char>(line[i]);
    if (c == '\x1b' && i + 1 < line.size() && line[i + 1] == '[') {
      i += 2;
      while (i < line.size() && line[i] != 'm') ++i;
      if (i < line.size()) ++i;
      continue;
    }
    size_t len = utf8_len(c);
    if (i + len > line.size()) len = 1;
    if (width == column) {
      line.insert(i + len, "\x1b[0m");
      line.insert(i, "\x1b[" + code + "m");
      return true;
    }
    i += len;
    ++width;
  }
  return false;
}

static bool valid_edit_range(std::size_t start, std::size_t end) {
  return start != npos && end != npos && start <= end;
}

static void set_edit_range(Block& block, std::size_t start, std::size_t end, int depth) {
  if (!valid_edit_range(start, end)) return;
  block.edit_start = start;
  block.edit_end = end;
  block.edit_depth = depth;
}

static void add_field_hit(Block& block, int row, int column, int width, std::size_t start, std::size_t end,
                          int depth) {
  if (!valid_edit_range(start, end) || row < 0 || width <= 0) return;
  block.hits.push_back({row, column, width, start, depth, start, end, true});
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

static Block txt_source(std::string s, std::size_t offset, int depth = 0) {
  Block out{{std::move(s)}, 0, 0};
  out.width = display_width(out.lines.front());
  if (offset != npos) {
    for (int col = 0; col < out.width; ++col) out.hits.push_back({0, col, 1, offset, depth});
    set_edit_range(out, offset, offset + 1, depth);
  }
  return out;
}

static Block txt_source_chars(std::string s, std::size_t offset, int depth = 0) {
  Block out{{std::move(s)}, 0, 0};
  out.width = display_width(out.lines.front());
  if (offset != npos) {
    int col = 0;
    for (size_t i = 0; i < out.lines.front().size();) {
      unsigned char c = static_cast<unsigned char>(out.lines.front()[i]);
      size_t len = utf8_len(c);
      if (i + len > out.lines.front().size()) len = 1;
      out.hits.push_back({0, col++, 1, offset + i, depth});
      i += len;
    }
    set_edit_range(out, offset, offset + out.lines.front().size(), depth);
  }
  return out;
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

static Block large_symbol(std::string name, std::size_t offset = npos, int depth = 0) {
  Block out;
  if (name == "sum") out = Block{{"\xE2\x95\xB2\xE2\x94\x80\xE2\x94\x80", " > ", "\xE2\x95\xB1\xE2\x94\x80\xE2\x94\x80"}, 3, 1};
  else if (name == "prod") out = Block{{"\xE2\x94\xAC \xE2\x94\xAC", "\xE2\x94\x82 \xE2\x94\x82", "\xE2\x94\xB4 \xE2\x94\xB4"}, 3, 1};
  else if (name == "int") out = Block{{"\xE2\x8C\xA0", "\xE2\x8E\xAE", "\xE2\x8C\xA1"}, 1, 1};
  else return txt_source(std::move(name), offset);
  if (offset != npos) {
    for (int row = 0; row < static_cast<int>(out.lines.size()); ++row) {
      for (int col = 0; col < out.width; ++col) out.hits.push_back({row, col, 1, offset, depth});
    }
    set_edit_range(out, offset, offset + name.size(), depth);
  }
  return out;
}

static Glyphs glyphs_for(const RenderOptions& opt) {
  if (opt.ascii) return {"-", "+", "+", "+", "+", "|", "-", "sqrt", "-"};
  return {"\xE2\x94\x80", "\xE2\x95\xAD", "\xE2\x95\xAE", "\xE2\x95\xB0", "\xE2\x95\xAF",
          "\xE2\x94\x82", "\xE2\x94\x80", "\xE2\x88\x9A", "\xE2\x94\x80"};
}

static std::string ansi(const std::string& text, const std::string& code, const RenderOptions& opt) {
  return opt.color ? "\x1b[" + code + "m" + text + "\x1b[0m" : text;
}

static std::string depth_color(int depth) {
  static const char* palette[] = {
      "38;5;229;1", "38;5;159;1", "38;5;219;1", "38;5;191;1",
      "38;5;215;1", "38;5;117;1", "38;5;183;1", "38;5;156;1"};
  constexpr std::size_t palette_size = sizeof(palette) / sizeof(palette[0]);
  return palette[static_cast<std::size_t>(std::max(0, depth)) % palette_size];
}

static std::vector<int> formula_depths(const std::string& text) {
  std::vector<int> depths(text.size(), 0);
  int group_depth = 0;
  int script_depth = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
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
  auto depths = formula_depths(line);
  for (size_t i = 0; i < line.size();) {
    unsigned char c = static_cast<unsigned char>(line[i]);
    if (c == '\x1b' && i + 1 < line.size() && line[i + 1] == '[') {
      size_t start = i;
      i += 2;
      while (i < line.size() && line[i] != 'm') ++i;
      if (i < line.size()) ++i;
      out.append(line, start, i - start);
      continue;
    }
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
    if (token == "^") out += ansi(token, "95;1", opt);
    else if (token == "_") out += ansi(token, "38;5;208;1", opt);
    else if (is_operator_glyph(token)) out += ansi(token, "33;1", opt);
    else if (is_delimiter_glyph(token)) out += ansi(token, depth_color(depths[i]), opt);
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
  out.hits = b.hits;
  for (auto& hit : out.hits) hit.row += top;
  out.edit_start = b.edit_start;
  out.edit_end = b.edit_end;
  out.edit_depth = b.edit_depth;
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
  out.hits = left.hits;
  for (auto hit : right.hits) {
    hit.column += left.width + gap;
    out.hits.push_back(hit);
  }
  if (valid_edit_range(a.edit_start, a.edit_end) && valid_edit_range(b.edit_start, b.edit_end)) {
    set_edit_range(out, std::min(a.edit_start, b.edit_start), std::max(a.edit_end, b.edit_end),
                   std::min(a.edit_depth, b.edit_depth));
  } else if (valid_edit_range(a.edit_start, a.edit_end)) {
    set_edit_range(out, a.edit_start, a.edit_end, a.edit_depth);
  } else if (valid_edit_range(b.edit_start, b.edit_end)) {
    set_edit_range(out, b.edit_start, b.edit_end, b.edit_depth);
  }
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
  out.hits = b.hits;
  for (auto& hit : out.hits) ++hit.column;
  out.edit_start = b.edit_start;
  out.edit_end = b.edit_end;
  out.edit_depth = b.edit_depth;
  return out;
}

static Block paren(const Block& b) {
  return hcat(hcat(txt("("), b, 0), txt(")"), 0);
}

static Block vfrac(const Block& n, const Block& d, const Glyphs& g) {
  int w = std::max(n.width, d.width) + 2;
  std::vector<int> offsets;
  auto center = [&](const Block& b) {
    std::vector<std::string> out;
    int left = (w - b.width) / 2;
    offsets.push_back(left);
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
  out.hits = n.hits;
  for (auto& hit : out.hits) hit.column += offsets[0];
  for (auto hit : d.hits) {
    hit.row += static_cast<int>(top.size()) + 1;
    hit.column += offsets[1];
    out.hits.push_back(hit);
  }
  if (valid_edit_range(n.edit_start, n.edit_end)) {
    for (int row = 0; row < static_cast<int>(top.size()); ++row) {
      add_field_hit(out, row, 0, w, n.edit_start, n.edit_end, n.edit_depth);
    }
  }
  if (valid_edit_range(d.edit_start, d.edit_end)) {
    int start_row = static_cast<int>(top.size()) + 1;
    for (int row = 0; row < static_cast<int>(bottom.size()); ++row) {
      add_field_hit(out, start_row + row, 0, w, d.edit_start, d.edit_end, d.edit_depth);
    }
  }
  if (valid_edit_range(n.edit_start, n.edit_end) && valid_edit_range(d.edit_start, d.edit_end)) {
    set_edit_range(out, std::min(n.edit_start, d.edit_start), std::max(n.edit_end, d.edit_end),
                   std::min(n.edit_depth, d.edit_depth));
  }
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
    out.hits = r.hits;
    for (auto& hit : out.hits) {
      hit.row += 1;
      hit.column += 2;
    }
    if (valid_edit_range(r.edit_start, r.edit_end)) {
      for (int row = 1; row < static_cast<int>(out.lines.size()); ++row) {
        add_field_hit(out, row, 2, r.width, r.edit_start, r.edit_end, r.edit_depth);
      }
      set_edit_range(out, r.edit_start, r.edit_end, r.edit_depth);
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
  out.hits = r.hits;
  for (auto& hit : out.hits) hit.column += 2;
  if (valid_edit_range(r.edit_start, r.edit_end)) {
    for (int row = 0; row < static_cast<int>(out.lines.size()); ++row) {
      add_field_hit(out, row, 2, r.width, r.edit_start, r.edit_end, r.edit_depth);
    }
    set_edit_range(out, r.edit_start, r.edit_end, r.edit_depth);
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
  out.hits = base.hits;
  for (auto& hit : out.hits) hit.row += static_cast<int>(exponent.lines.size());
  for (auto hit : exponent.hits) {
    hit.column += base.width;
    out.hits.push_back(hit);
  }
  if (valid_edit_range(base.edit_start, base.edit_end)) {
    int start_row = static_cast<int>(exponent.lines.size());
    for (int row = 0; row < static_cast<int>(base.lines.size()); ++row) {
      add_field_hit(out, start_row + row, 0, base.width, base.edit_start, base.edit_end, base.edit_depth);
    }
  }
  if (valid_edit_range(exponent.edit_start, exponent.edit_end)) {
    for (int row = 0; row < static_cast<int>(exponent.lines.size()); ++row) {
      add_field_hit(out, row, base.width, exponent.width, exponent.edit_start, exponent.edit_end,
                    exponent.edit_depth);
    }
  }
  if (valid_edit_range(base.edit_start, base.edit_end) && valid_edit_range(exponent.edit_start, exponent.edit_end)) {
    set_edit_range(out, std::min(base.edit_start, exponent.edit_start), std::max(base.edit_end, exponent.edit_end),
                   std::min(base.edit_depth, exponent.edit_depth));
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
  out.hits = base.hits;
  for (auto hit : sub.hits) {
    hit.row += static_cast<int>(base.lines.size());
    hit.column += base.width;
    out.hits.push_back(hit);
  }
  if (valid_edit_range(base.edit_start, base.edit_end)) {
    for (int row = 0; row < static_cast<int>(base.lines.size()); ++row) {
      add_field_hit(out, row, 0, base.width, base.edit_start, base.edit_end, base.edit_depth);
    }
  }
  if (valid_edit_range(sub.edit_start, sub.edit_end)) {
    int start_row = static_cast<int>(base.lines.size());
    for (int row = 0; row < static_cast<int>(sub.lines.size()); ++row) {
      add_field_hit(out, start_row + row, base.width, sub.width, sub.edit_start, sub.edit_end, sub.edit_depth);
    }
  }
  if (valid_edit_range(base.edit_start, base.edit_end) && valid_edit_range(sub.edit_start, sub.edit_end)) {
    set_edit_range(out, std::min(base.edit_start, sub.edit_start), std::max(base.edit_end, sub.edit_end),
                   std::min(base.edit_depth, sub.edit_depth));
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
                              char parent_op, int depth = 0) {
  Block out;
  if (auto n = dynamic_cast<const Number*>(&e)) return txt_source_chars(std::to_string(n->value), n->start, depth);
  if (auto d = dynamic_cast<const Decimal*>(&e)) return txt_source_chars(d->text, d->start, depth);
  if (auto s = dynamic_cast<const Symbol*>(&e)) {
    return ascii ? txt_source(render_symbol(s->name, true), s->start, depth) : large_symbol(s->name, s->start, depth);
  }

  if (auto fn = dynamic_cast<const FunctionCall*>(&e)) {
    auto arg = render_expr_prec(*fn->arg, g, ascii, 0, false, 0, depth + 1);
    if (fn->name == "fact") out = hcat(arg, txt_source("!", fn->end == npos ? npos : fn->end - 1, depth), 0);
    else if (fn->name == "percent") out = hcat(arg, txt_source("%", fn->end == npos ? npos : fn->end - 1, depth), 0);
    else if (fn->name == "abs") out = hcat(hcat(txt_source("|", fn->start, depth), arg, 0), txt_source("|", fn->end == npos ? npos : fn->end - 1, depth), 0);
    else out = hcat(txt_source(fn->name, fn->start, depth), paren(arg, ascii), 0);
  } else if (auto f = dynamic_cast<const Fraction*>(&e)) {
    out = vfrac(render_expr_prec(*f->num, g, ascii, 0, false, 0, depth + 1),
                render_expr_prec(*f->den, g, ascii, 0, false, 0, depth + 1), g);
  } else if (auto q = dynamic_cast<const Sqrt*>(&e)) {
    auto radicand = render_expr_prec(*q->radicand, g, ascii, 0, false, 0, depth + 1);
    if (q->index) {
      auto index = render_expr_prec(*q->index, g, ascii, 0, false, 0, depth + 1);
      out = vroot(radicand, &index, g, ascii);
    } else {
      out = vroot(radicand, nullptr, g, ascii);
    }
  } else if (auto p = dynamic_cast<const Superscript*>(&e)) {
    int base_depth = (dynamic_cast<const Superscript*>(p->base.get()) ||
                      dynamic_cast<const Subscript*>(p->base.get()))
                         ? depth + 1
                         : depth;
    auto base = render_expr_prec(*p->base, g, ascii, 0, false, 0, base_depth);
    auto exponent = render_expr_prec(*p->exponent, g, ascii, 0, false, 0, depth + 1);
    if (needs_script_grouping(*p->base, base, '^')) base = paren(base, ascii);
    out = superscript(base, exponent, ascii);
  } else if (auto sub = dynamic_cast<const Subscript*>(&e)) {
    int base_depth = (dynamic_cast<const Superscript*>(sub->base.get()) ||
                      dynamic_cast<const Subscript*>(sub->base.get()))
                         ? depth + 1
                         : depth;
    auto base = render_expr_prec(*sub->base, g, ascii, 0, false, 0, base_depth);
    auto subscript = render_expr_prec(*sub->sub, g, ascii, 0, false, 0, depth + 1);
    if (needs_script_grouping(*sub->base, base, '_')) base = paren(base, ascii);
    out = subscript_block(base, subscript, ascii);
  } else if (auto b = dynamic_cast<const Binary*>(&e)) {
    if (b->op == '*') {
      if (auto n = dynamic_cast<const Number*>(b->lhs.get()); n && n->value == -1) {
        out = hcat(txt_source("-", b->start, depth), render_expr_prec(*b->rhs, g, ascii, 3, false, '-', depth), 0);
        return needs_paren(e, parent_prec, right_child, parent_op) ? paren(out, ascii) : out;
      }
    }
    int prec = precedence(e);
    if (b->op == '/') {
      out = vfrac(render_expr_prec(*b->lhs, g, ascii, 0, false, 0, depth + 1),
                  render_expr_prec(*b->rhs, g, ascii, 0, false, 0, depth + 1), g);
    } else {
      std::string op = b->op == '*' && !ascii ? "\xC2\xB7" : std::string(1, b->op);
      out = hcat(hcat(render_expr_prec(*b->lhs, g, ascii, prec, false, b->op, depth), txt_source(op, b->lhs->end, depth)),
                 render_expr_prec(*b->rhs, g, ascii, prec, true, b->op, depth));
    }
  } else {
    out = txt("?");
  }

  return needs_paren(e, parent_prec, right_child, parent_op) ? paren(out, ascii) : out;
}

static Block render_expr(const Expr& e, const Glyphs& g, bool ascii) {
  return render_expr_prec(e, g, ascii, 0, false, 0);
}

static void shift_hits(Block& block, std::size_t offset) {
  for (auto& hit : block.hits) {
    hit.offset += offset;
    if (hit.field) {
      hit.edit_start += offset;
      hit.edit_end += offset;
    }
  }
  if (valid_edit_range(block.edit_start, block.edit_end)) {
    block.edit_start += offset;
    block.edit_end += offset;
  }
}

static std::string trim_right(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
  return text;
}

static bool trailing_operator(char c) {
  return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '_' || c == ',';
}

static std::string complete_for_preview(std::string text) {
  text = trim_right(std::move(text));
  while (!text.empty() && trailing_operator(text.back())) {
    text.pop_back();
    text = trim_right(std::move(text));
  }

  std::vector<char> stack;
  for (char c : text) {
    if (c == '(' || c == '{' || c == '[') stack.push_back(c);
    else if ((c == ')' || c == '}' || c == ']') && !stack.empty()) stack.pop_back();
  }
  while (!stack.empty()) {
    char c = stack.back();
    stack.pop_back();
    if (c == '(') text.push_back(')');
    else if (c == '{') text.push_back('}');
    else if (c == '[') text.push_back(']');
  }
  return text;
}

static std::optional<Block> try_render_preview_slice(const std::string& input, std::size_t start, std::size_t length,
                                                     const Glyphs& g, bool ascii) {
  bool alpha_only = true;
  for (std::size_t i = start; i < start + length; ++i) {
    if (!std::isalpha(static_cast<unsigned char>(input[i]))) {
      alpha_only = false;
      break;
    }
  }
  if (alpha_only) {
    std::size_t command_start = start;
    while (command_start > 0 && std::isalpha(static_cast<unsigned char>(input[command_start - 1]))) --command_start;
    if (command_start > 0 && input[command_start - 1] == '\\') return std::nullopt;
  }
  std::string candidate = complete_for_preview(input.substr(start, length));
  if (candidate.empty()) return std::nullopt;
  try {
    auto ast = parse(candidate);
    auto block = render_expr(*ast, g, ascii);
    shift_hits(block, start);
    return block;
  } catch (...) {
    return std::nullopt;
  }
}

static Block raw_preview_block(const std::string& input) {
  Block out{{input.empty() ? std::string(" ") : input}, std::max(1, display_width(input)), 0};
  int col = 0;
  for (size_t i = 0; i < input.size();) {
    unsigned char c = static_cast<unsigned char>(input[i]);
    size_t len = utf8_len(c);
    if (i + len > input.size()) len = 1;
    out.hits.push_back({0, col++, 1, i, 0});
    i += len;
  }
  if (out.hits.empty()) out.hits.push_back({0, 0, 1, 0, 0});
  return out;
}

static void colorize_source_depth(std::vector<std::string>& lines, const std::vector<SourceHit>& hits,
                                  const RenderOptions& opt) {
  if (!opt.color) return;
  struct Cell {
    int row = 0;
    int column = 0;
    int depth = 0;
  };
  std::vector<Cell> cells;
  cells.reserve(hits.size());
  for (const auto& hit : hits) {
    if (hit.field) continue;
    if (hit.row < 0 || hit.row >= static_cast<int>(lines.size())) continue;
    for (int col = hit.column; col < hit.column + std::max(1, hit.width); ++col) {
      cells.push_back({hit.row, col, hit.depth});
    }
  }
  std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
    if (a.row != b.row) return a.row < b.row;
    if (a.column != b.column) return a.column < b.column;
    return a.depth < b.depth;
  });
  cells.erase(std::unique(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
                return a.row == b.row && a.column == b.column;
              }),
              cells.end());
  for (const auto& cell : cells) style_display_column(lines[cell.row], cell.column, depth_color(cell.depth), opt);
}

static void overlay_source_caret(std::vector<std::string>& lines, const std::vector<SourceHit>& hits,
                                 std::size_t cursor, const RenderOptions& opt) {
  if (lines.empty() || hits.empty()) return;
  const SourceHit* best = nullptr;
  std::size_t best_distance = static_cast<std::size_t>(-1);
  for (const auto& hit : hits) {
    if (hit.field) continue;
    std::size_t distance = hit.offset > cursor ? hit.offset - cursor : cursor - hit.offset;
    if (!best || distance < best_distance || (distance == best_distance && hit.offset <= cursor)) {
      best = &hit;
      best_distance = distance;
    }
  }
  if (!best || best->row < 0 || best->row >= static_cast<int>(lines.size())) return;
  int column = best->column;
  if (!style_display_column(lines[best->row], column, "48;5;238", opt) && lines[best->row].empty()) {
    lines[best->row] = " ";
    style_display_column(lines[best->row], 0, "48;5;238", opt);
  }
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

RenderedEquation render_equation(const std::string& input, const RenderOptions& options) {
  auto g = glyphs_for(options);
  RenderedEquation out;
  auto ast = parse(input);
  auto block = render_expr(*ast, g, options.ascii);
  out.lines = std::move(block.lines);
  out.width = block.width;
  out.hits = std::move(block.hits);
  return out;
}

RenderedEquation render_equation_best_effort(const std::string& input, const RenderOptions& options) {
  try {
    return render_equation(input, options);
  } catch (...) {
  }

  auto g = glyphs_for(options);
  for (std::size_t length = input.size(); length > 0; --length) {
    for (std::size_t start = 0; start + length <= input.size(); ++start) {
      if (auto block = try_render_preview_slice(input, start, length, g, options.ascii)) {
        return {std::move(block->lines), block->width, std::move(block->hits)};
      }
    }
  }

  auto block = raw_preview_block(input);
  return {std::move(block.lines), block.width, std::move(block.hits)};
}

std::optional<std::size_t> source_offset_at(const RenderedEquation& equation, int row, int column) {
  std::optional<std::size_t> direct;
  int direct_width = 1000000;
  for (const auto& hit : equation.hits) {
    if (hit.field || hit.row != row) continue;
    if (column < hit.column || column >= hit.column + hit.width) continue;
    if (hit.width < direct_width) {
      direct = hit.offset + static_cast<std::size_t>(std::clamp(column - hit.column, 0, hit.width - 1));
      direct_width = hit.width;
    }
  }
  if (direct) return direct;

  const SourceHit* field = nullptr;
  int field_score = 1000000;
  for (const auto& hit : equation.hits) {
    if (!hit.field || !valid_edit_range(hit.edit_start, hit.edit_end)) continue;
    if (hit.row != row || column < hit.column || column >= hit.column + hit.width) continue;
    int span = std::max(0, static_cast<int>(hit.edit_end - hit.edit_start));
    int rel = std::clamp(column - hit.column, 0, std::max(0, hit.width - 1));
    int area_score = hit.width + span;
    if (!field || area_score < field_score) {
      field = &hit;
      field_score = area_score;
    }
  }
  if (field) {
    if (field->edit_start == field->edit_end || field->width <= 1) return field->edit_start;
    int span = static_cast<int>(field->edit_end - field->edit_start);
    int rel = std::clamp(column - field->column, 0, field->width - 1);
    int offset = (rel * span + (field->width - 1) / 2) / std::max(1, field->width - 1);
    return field->edit_start + static_cast<std::size_t>(std::clamp(offset, 0, span));
  }

  std::optional<std::size_t> best;
  int best_distance = 1000000;
  for (const auto& hit : equation.hits) {
    if (hit.field) continue;
    int row_distance = std::abs(hit.row - row);
    int column_distance = 0;
    if (column < hit.column) column_distance = hit.column - column;
    else if (column >= hit.column + hit.width) column_distance = column - (hit.column + hit.width - 1);
    int distance = row_distance * 1000 + column_distance;
    if (distance < best_distance) {
      best_distance = distance;
      best = hit.offset;
    }
  }
  return best;
}

std::string render_boxed(const std::string& input, const std::string& result, const RenderOptions& options,
                         const std::string& extra, const std::string& extra_title,
                         std::optional<std::size_t> cursor) {
  auto g = glyphs_for(options);
  std::vector<std::string> pretty;
  auto equation = render_equation_best_effort(input, options);
  pretty = equation.lines;
  colorize_source_depth(pretty, equation.hits, options);
  if (cursor) overlay_source_caret(pretty, equation.hits, *cursor, options);
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
  out << panel("Pretty Render", pretty, width, g, options, "35", "", Align::Center);
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
