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
static std::string sp(int n) { return std::string(std::max(0, n), ' '); }
static std::string rep(const std::string& g, int n) { std::string o; for (int i=0;i<std::max(0,n);++i) o += g; return o; }
static Block txt(std::string s) { return Block{{std::move(s)}, static_cast<int>(s.size()), 0}; }

struct Glyphs {
  std::string h, tl, tr, bl, br, v, title;
  std::string sqrt, frac;
};

Glyphs glyphs_for(const RenderOptions& opt) {
  if (opt.ascii) return {"-", "+", "+", "+", "+", "|", "-", "sqrt", "-"};
  return {"─", "╭", "╮", "╰", "╯", "│", "─", "√", "─"};
}

static Block pad(const Block& b, int top, int bot, int w) {
  Block o; o.width = w; o.baseline = b.baseline + top;
  for (int i=0;i<top;++i) o.lines.push_back(sp(w));
  for (auto line : b.lines) { line += sp(w - static_cast<int>(line.size())); o.lines.push_back(std::move(line)); }
  for (int i=0;i<bot;++i) o.lines.push_back(sp(w));
  return o;
}

static Block hcat(const Block& a, const Block& b, int gap = 1) {
  int top = std::max(a.baseline, b.baseline);
  int ab = static_cast<int>(a.lines.size()) - a.baseline - 1;
  int bb = static_cast<int>(b.lines.size()) - b.baseline - 1;
  int bot = std::max(ab, bb);
  Block aa = pad(a, top - a.baseline, bot - ab, a.width);
  Block bbp = pad(b, top - b.baseline, bot - bb, b.width);
  Block o; o.width = aa.width + gap + bbp.width; o.baseline = top; o.lines.resize(aa.lines.size());
  for (size_t i=0;i<o.lines.size();++i) o.lines[i] = aa.lines[i] + sp(gap) + bbp.lines[i];
  return o;
}

static Block vfrac(const Block& n, const Block& d, const Glyphs& g) {
  int w = std::max(n.width, d.width) + 2;
  auto center = [&](const Block& b){ std::vector<std::string> out; int left=(w-b.width)/2; for(auto line:b.lines) out.push_back(sp(left)+line+sp(w-left-static_cast<int>(line.size()))); return out; };
  Block o; auto t=center(n); auto bt=center(d);
  o.lines.insert(o.lines.end(), t.begin(), t.end());
  o.lines.push_back(rep(g.frac, w));
  o.lines.insert(o.lines.end(), bt.begin(), bt.end());
  o.width = w; o.baseline = static_cast<int>(t.size());
  return o;
}

static Block vsqrt(const Block& r, const Glyphs& g) {
  Block o; o.width = r.width + 2; o.baseline = r.baseline;
  for (size_t i = 0; i < r.lines.size(); ++i) {
    if (i == 0) o.lines.push_back(g.sqrt + std::string(" ") + r.lines[i]);
    else o.lines.push_back(std::string(2, ' ') + r.lines[i]);
  }
  return o;
}

static Block render_expr(const Expr& e, const Glyphs& g) {
  if (auto n = dynamic_cast<const Number*>(&e)) return txt(std::to_string(n->value));
  if (auto s = dynamic_cast<const Symbol*>(&e)) return txt(s->name);
  if (auto f = dynamic_cast<const Fraction*>(&e)) return vfrac(render_expr(*f->num, g), render_expr(*f->den, g), g);
  if (auto q = dynamic_cast<const Sqrt*>(&e)) return vsqrt(render_expr(*q->radicand, g), g);
  if (auto p = dynamic_cast<const Superscript*>(&e)) {
    auto b = render_expr(*p->base, g);
    auto ex = render_expr(*p->exponent, g);
    ex = pad(ex, 0, std::max(0, (int)b.lines.size() - (int)ex.lines.size()), ex.width);
    b = pad(b, std::max(0, (int)ex.lines.size() - 1), 0, b.width);
    return hcat(b, ex, 0);
  }
  if (auto sb = dynamic_cast<const Subscript*>(&e)) {
    auto b = render_expr(*sb->base, g);
    auto sub = render_expr(*sb->sub, g);
    b = pad(b, 0, std::max(0, (int)sub.lines.size()), b.width);
    sub = pad(sub, std::max(0, (int)b.lines.size() - (int)sub.lines.size()), 0, sub.width);
    return hcat(b, sub, 0);
  }
  auto b = dynamic_cast<const Binary*>(&e);
  if (!b) return txt("?");
  return hcat(hcat(render_expr(*b->lhs, g), txt(std::string(1, b->op))), render_expr(*b->rhs, g));
}

static std::string panel(const std::string& title, const std::vector<std::string>& content, int width, const Glyphs& g) {
  std::ostringstream o;
  std::string top = g.tl + g.title + " " + title + " ";
  if ((int)top.size() < width - 1) top += rep(g.h, width - 1 - (int)top.size());
  o << top << g.tr << "\n";
  for (auto line : content) {
    if ((int)line.size() > width - 4) line = line.substr(0, width - 4);
    o << g.v << " " << line << sp(width - 4 - (int)line.size()) << " " << g.v << "\n";
  }
  o << g.bl << rep(g.h, width - 2) << g.br << "\n";
  return o.str();
}

} // namespace

std::string render_boxed(const std::string& input, const std::string& result, const RenderOptions& options) {
  auto g = glyphs_for(options);
  std::vector<std::string> pretty;
  try { auto ast = parse(input); pretty = render_expr(*ast, g).lines; }
  catch (...) { pretty = {input}; }
  int width = 68;
  std::ostringstream o;
  o << panel("OverCalc / Input", {input}, width, g);
  o << panel("Pretty Render", pretty, width, g);
  o << panel("Result", {result}, width, g);
  return o.str();
}

} // namespace overcalc
