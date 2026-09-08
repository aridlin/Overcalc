#pragma once
#include <optional>
#include <string>
#include <vector>

namespace overcalc {

struct RenderOptions {
  bool ascii = false;
  bool color = true;
  int panel_width = 88;
};

struct SourceHit {
  int row = 0;
  int column = 0;
  int width = 1;
  std::size_t offset = 0;
  int depth = 0;
  std::size_t edit_start = 0;
  std::size_t edit_end = 0;
  bool field = false;
};

struct RenderedEquation {
  std::vector<std::string> lines;
  int width = 0;
  std::vector<SourceHit> hits;
};

RenderedEquation render_equation(const std::string& input, const RenderOptions& options = {});
RenderedEquation render_equation_best_effort(const std::string& input, const RenderOptions& options = {});
std::optional<std::size_t> source_offset_at(const RenderedEquation& equation, int row, int column);
std::string render_boxed(const std::string& input, const std::string& result, const RenderOptions& options = {},
                         const std::string& extra = "", const std::string& extra_title = "Simplified",
                         std::optional<std::size_t> cursor = std::nullopt);
std::string render_steps_boxed(const std::vector<std::string>& steps, const RenderOptions& options = {});

}  // namespace overcalc
