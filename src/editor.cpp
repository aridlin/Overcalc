#include "overcalc/editor.hpp"
#include <algorithm>
#include <cctype>
#include <limits>

namespace overcalc {
const std::vector<EditTemplate>& edit_templates() {
  static const std::vector<EditTemplate> values = {
    {"Fraction", "frac", "", 2}, {"Superscript", "power", "", 2},
    {"Square root", "sqrt", "", 1}, {"Subscript", "sub", "", 2},
    {"Brackets", "group", "", 1}, {"Nth root", "root", "", 2},
    {"Absolute", "abs", "", 1}, {"sin", "func", "sin", 1},
    {"cos", "func", "cos", 1}, {"tan", "func", "tan", 1},
    {"ln", "func", "ln", 1}, {"log", "func", "log", 1},
    {"pi", "text", "pi", 0}, {"e", "text", "e", 0},
    {"Multiply", "text", "*", 0}, {"Percent", "text", "%", 0},
    {"Factorial", "text", "!", 0}, {"alpha", "text", "alpha", 0},
    {"beta", "text", "beta", 0}, {"theta", "text", "theta", 0}
  };
  return values;
}
namespace {
std::string source_field(const EditField& f) {
  std::string s;
  for (const auto& i : f.items) {
    auto a = [&](int n) { return source_field(*i.fields.at(n)); };
    if (i.kind == "text") s += i.text;
    else if (i.kind == "frac") s += "\\frac{" + a(0) + "}{" + a(1) + "}";
    else if (i.kind == "power" || i.kind == "sub") s += "(" + a(0) + ")" + (i.kind == "power" ? "^{" : "_{") + a(1) + "}";
    else if (i.kind == "sqrt") s += "\\sqrt{" + a(0) + "}";
    else if (i.kind == "root") s += "\\sqrt[" + a(0) + "]{" + a(1) + "}";
    else if (i.kind == "group") s += "(" + a(0) + ")";
    else if (i.kind == "abs") s += "abs(" + a(0) + ")";
    else s += i.text + "(" + a(0) + ")";
  }
  return s;
}
bool contains_field(const std::shared_ptr<EditField>& root, const std::shared_ptr<EditField>& target) {
  if (root == target) return true;
  for (const auto& i : root->items) for (const auto& c : i.fields) if (contains_field(c, target)) return true;
  return false;
}
bool complete_field(const EditField& f) {
  if (f.items.empty()) return false;
  for (const auto& i : f.items) for (const auto& c : i.fields) if (!complete_field(*c)) return false;
  return true;
}
std::u32string glyphs(const std::string& text) {
  if(text=="pi") return U"π";
  if(text=="alpha") return U"α";
  if(text=="beta") return U"β";
  if(text=="theta") return U"θ";
  if(text=="*") return U" × ";
  if(text=="+") return U" + ";
  if(text=="-") return U" − ";
  return {text.begin(),text.end()};
}
EditLayout box(int w, int h, int b) { return {std::vector<std::u32string>(h, std::u32string(w, U' ')), {}, w, b}; }
void paste(EditLayout& dst, const EditLayout& src, int x, int y) {
  for (std::size_t r=0;r<src.lines.size();++r) dst.lines[y+r].replace(x,src.width,src.lines[r]);
  for(auto c:src.carets) { c.x+=x; c.y+=y; dst.carets.push_back(c); }
}
EditLayout render_field(const std::shared_ptr<EditField>& f);
EditLayout tight(EditLayout out) {
  bool trailing=out.width>1;
  for(const auto& line:out.lines) trailing=trailing && line.back()==U' ';
  if(trailing) {--out.width;for(auto& line:out.lines) line.pop_back();}
  return out;
}
EditLayout render_item(const EditItem& i) {
  if(i.kind=="text") { auto t=glyphs(i.text); return {{t},{},static_cast<int>(t.size()),0}; }
  auto a=tight(render_field(i.fields[0]));
  int ah=static_cast<int>(a.lines.size());
  if(i.kind=="frac") {
    auto b=tight(render_field(i.fields[1]));
    int w=std::max(a.width,b.width)+2;
    auto out=box(w,ah+1+b.lines.size(),ah);
    paste(out,a,(w-a.width)/2,0);
    out.lines[ah]=std::u32string(w,U'─');
    paste(out,b,(w-b.width)/2,ah+1);
    return out;
  }
  if(i.kind=="power" || i.kind=="sub") {
    auto b=tight(render_field(i.fields[1]));
    int bh=static_cast<int>(b.lines.size());
    bool power=i.kind=="power";
    auto out=box(a.width+b.width,ah+bh,a.baseline+(power?bh:0));
    paste(out,a,0,power?bh:0);
    paste(out,b,a.width,power?0:ah);
    return out;
  }
  if(i.kind=="sqrt" || i.kind=="root") {
    auto rad=i.kind=="root"?tight(render_field(i.fields[1])):a;
    int idxw=i.kind=="root"?a.width:0;
    int top=i.kind=="root"?std::max(1,ah):1;
    auto out=box(idxw+2+rad.width,top+rad.lines.size(),top+rad.baseline);
    if(i.kind=="root") paste(out,a,0,0);
    out.lines[top-1].replace(idxw+1,rad.width+1,std::u32string(rad.width+1,U'‾'));
    for(int y=top;y<static_cast<int>(out.lines.size())-1;++y) out.lines[y][idxw+1]=U'│';
    out.lines.back()[idxw]=U'√';
    out.lines.back()[idxw+1]=U'│';
    if(rad.lines.size()==1) {out.lines.back()[idxw]=U' ';out.lines.back()[idxw+1]=U'√';}
    paste(out,rad,idxw+2,top);
    return out;
  }
  std::u32string name=i.kind=="func"?glyphs(i.text):U"";
  int left=static_cast<int>(name.size());
  auto out=box(left+a.width+2,ah,a.baseline);
  paste(out,a,left+1,0);
  out.lines[a.baseline].replace(0,name.size(),name);
  for(int y=0;y<ah;++y) {
    char32_t l=U'(',r=U')';
    if(i.kind=="abs") l=r=U'│';
    else if(ah>1) {l=y==0?U'⎛':y==ah-1?U'⎝':U'⎜'; r=y==0?U'⎞':y==ah-1?U'⎠':U'⎟';}
    out.lines[y][left]=l;out.lines[y].back()=r;
  }
  return out;
}
EditLayout render_field(const std::shared_ptr<EditField>& f) {
  if(f->items.empty()) return {{U"□"},{{f,0,0,0}},1,0};
  std::vector<EditLayout> items;
  int above=0,below=0,width=1;
  for(const auto& i:f->items) {
    auto l=render_item(i);
    above=std::max(above,l.baseline);
    below=std::max(below,static_cast<int>(l.lines.size())-l.baseline-1);
    width+=l.width;items.push_back(std::move(l));
  }
  auto out=box(width,above+below+1,above);
  int x=0;
  for(std::size_t n=0;n<items.size();++n) {
    out.carets.push_back({f,n,x,above});
    paste(out,items[n],x,above-items[n].baseline);x+=items[n].width;
  }
  out.carets.push_back({f,items.size(),x,above});return out;
}
std::shared_ptr<EditField> clone(const std::shared_ptr<EditField>& f, const std::shared_ptr<EditField>& active,
                               std::shared_ptr<EditField>& target) {
  auto copy = std::make_shared<EditField>();
  if (f == active) target = copy;
  for (auto i : f->items) {
    for (auto& c : i.fields) c = clone(c, active, target);
    copy->items.push_back(std::move(i));
  }
  return copy;
}
}
std::string editor_utf8(const std::u32string& text) {
  std::string out;
  for(char32_t c:text) {
    if(c<0x80) out+=static_cast<char>(c);
    else if(c<0x800) {out+=static_cast<char>(0xc0|(c>>6));out+=static_cast<char>(0x80|(c&63));}
    else if(c<0x10000) {out+=static_cast<char>(0xe0|(c>>12));out+=static_cast<char>(0x80|((c>>6)&63));out+=static_cast<char>(0x80|(c&63));}
    else {out+=static_cast<char>(0xf0|(c>>18));out+=static_cast<char>(0x80|((c>>12)&63));out+=static_cast<char>(0x80|((c>>6)&63));out+=static_cast<char>(0x80|(c&63));}
  }
  return out;
}
ExpressionEditor::ExpressionEditor() : root_(std::make_shared<EditField>()), field_(root_) {}
ExpressionEditor::Snapshot ExpressionEditor::snapshot() const {
  Snapshot s; s.root = clone(root_, field_, s.field); s.pos = pos_; return s;
}
void ExpressionEditor::save() { undo_.push_back(snapshot()); if (undo_.size()>200) undo_.erase(undo_.begin()); redo_.clear(); }
void ExpressionEditor::undo(bool redo) {
  auto& from = redo ? redo_ : undo_; auto& to = redo ? undo_ : redo_;
  if (from.empty()) return;
  to.push_back(snapshot()); auto s = from.back(); from.pop_back(); root_=s.root; field_=s.field; pos_=s.pos;
}
void ExpressionEditor::clear() { save(); root_=std::make_shared<EditField>(); field_=root_; pos_=0; }
void ExpressionEditor::type(char c) {
  if (c == '/') { insert(0); return; }
  if (c == '^') { insert(1); return; }
  if (c == '_') { insert(3); return; }
  if (c == '(') { insert(4); return; }
  if (c == ')') { tab(); return; }
  if (!std::isalnum(static_cast<unsigned char>(c)) && std::string(".+-*%! ").find(c)==std::string::npos) return;
  save(); field_->items.insert(field_->items.begin()+pos_, {"text", std::string(1,c), {}}); ++pos_;
}
void ExpressionEditor::insert(std::size_t index) {
  if (index>=edit_templates().size()) return;
  save(); const auto& t = edit_templates()[index]; EditItem i{t.kind,t.text,{}};
  for (int n=0;n<t.fields;++n) i.fields.push_back(std::make_shared<EditField>());
  bool wrapped = false;
  if ((t.kind=="frac" || t.kind=="power" || t.kind=="sub") && pos_>0) {
    auto start=pos_-1;
    if (field_->items[start].kind=="text" && std::isalnum(static_cast<unsigned char>(field_->items[start].text[0]))) {
      while (start>0 && field_->items[start-1].kind=="text" &&
             (std::isalnum(static_cast<unsigned char>(field_->items[start-1].text[0])) || field_->items[start-1].text==".")) --start;
    }
    const auto& prev=field_->items[start];
    if (prev.kind!="text" || std::isalnum(static_cast<unsigned char>(prev.text[0])) || prev.text==".") {
      i.fields[0]->items.assign(field_->items.begin()+start,field_->items.begin()+pos_);
      field_->items.erase(field_->items.begin()+start,field_->items.begin()+pos_); pos_=start; wrapped=true;
    }
  }
  field_->items.insert(field_->items.begin()+pos_,i);
  if (i.fields.empty()) ++pos_;
  else { field_=i.fields[wrapped ? 1 : 0]; pos_=field_->items.size(); }
}
void ExpressionEditor::erase(bool backward) {
  if ((backward && pos_==0) || (!backward && pos_==field_->items.size())) { move(backward ? -1 : 1); return; }
  save(); if (backward) --pos_; field_->items.erase(field_->items.begin()+pos_);
}
EditLayout ExpressionEditor::layout() const { return render_field(root_); }
EditCaret ExpressionEditor::caret() const {
  for (const auto& c:layout().carets) if(c.field==field_ && c.pos==pos_) return c;
  return {field_,pos_,0,0};
}
void ExpressionEditor::move(int direction) {
  auto cs=layout().carets;
  for (std::size_t n=0;n<cs.size();++n) if(cs[n].field==field_ && cs[n].pos==pos_) {
    int next=std::clamp(static_cast<int>(n)+direction,0,static_cast<int>(cs.size())-1);
    field_=cs[next].field; pos_=cs[next].pos; return;
  }
}
void ExpressionEditor::tab(bool backward) {
  auto cs=layout().carets;
  for (int n=0;n<static_cast<int>(cs.size());++n) if(cs[n].field==field_ && cs[n].pos==pos_) {
    for(int j=n+(backward?-1:1);j>=0 && j<static_cast<int>(cs.size());j+=backward?-1:1)
      if(cs[j].field!=field_) { field_=cs[j].field; pos_=cs[j].pos; return; }
    home(!backward); return;
  }
}
void ExpressionEditor::vertical(int direction) {
  auto at=caret(); auto best=at; int score=std::numeric_limits<int>::max();
  for(const auto& c:layout().carets) if((c.y-at.y)*direction>0 && !contains_field(c.field, field_)) {
    int d=std::abs(c.y-at.y)*1000+std::abs(c.x-at.x);
    if(d<score) { score=d; best=c; }
  }
  field_=best.field; pos_=best.pos;
}
void ExpressionEditor::home(bool end) { pos_=end ? field_->items.size() : 0; }
void ExpressionEditor::click(int x,int y) {
  int score=std::numeric_limits<int>::max();
  for(const auto& c:layout().carets) {
    int d=std::abs(c.y-y)*1000+std::abs(c.x-x);
    if(d<score) { score=d; field_=c.field; pos_=c.pos; }
  }
}
std::string ExpressionEditor::source() const { return source_field(*root_); }
bool ExpressionEditor::complete() const { return complete_field(*root_); }
}
