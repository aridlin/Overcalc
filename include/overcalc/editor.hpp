#pragma once
#include <memory>
#include <string>
#include <vector>

namespace overcalc {
struct EditField;
struct EditItem {
  std::string kind, text;
  std::vector<std::shared_ptr<EditField>> fields;
};
struct EditField { std::vector<EditItem> items; };
struct EditCaret { std::shared_ptr<EditField> field; std::size_t pos; int x, y; };
struct EditLayout {
  std::vector<std::u32string> lines;
  std::vector<EditCaret> carets;
  int width = 0, baseline = 0;
};
struct EditTemplate { std::string label, kind, text; int fields; };
const std::vector<EditTemplate>& edit_templates();
std::string editor_utf8(const std::u32string& text);
class ExpressionEditor {
 public:
  ExpressionEditor();
  void type(char c);
  void insert(std::size_t index);
  void erase(bool backward);
  void move(int direction);
  void tab(bool backward = false);
  void vertical(int direction);
  void home(bool end);
  void click(int x, int y);
  void clear();
  void undo(bool redo = false);
  std::string source() const;
  bool complete() const;
  EditLayout layout() const;
  EditCaret caret() const;
 private:
  std::shared_ptr<EditField> root_, field_;
  std::size_t pos_ = 0;
  struct Snapshot { std::shared_ptr<EditField> root, field; std::size_t pos; };
  std::vector<Snapshot> undo_, redo_;
  Snapshot snapshot() const;
  void save();
};
}
