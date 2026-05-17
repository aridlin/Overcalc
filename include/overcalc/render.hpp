#pragma once
#include <string>
#include <vector>

namespace overcalc {

struct RenderOptions {
  bool ascii = false;
  bool color = true;
  int panel_width = 88;
};

std::string render_boxed(const std::string& input, const std::string& result, const RenderOptions& options = {},
                         const std::string& extra = "", const std::string& extra_title = "Simplified");
std::string render_steps_boxed(const std::vector<std::string>& steps, const RenderOptions& options = {});

}  // namespace overcalc
