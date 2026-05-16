#pragma once
#include <string>

namespace overcalc {

struct RenderOptions {
  bool ascii = false;
};

std::string render_boxed(const std::string& input, const std::string& result, const RenderOptions& options = {});

}  // namespace overcalc
