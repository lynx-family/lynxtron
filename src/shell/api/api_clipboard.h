// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_API_API_CLIPBOARD_H_
#define LYNXTRON_SHELL_API_API_CLIPBOARD_H_

#include <optional>
#include <string>
#include <vector>

#include "ui/gfx/image/image.h"

namespace lynxtron::api::clipboard {

struct ClipboardData {
  std::optional<std::string> text;
  std::optional<std::string> html;
  std::optional<gfx::Image> image;
};

std::vector<std::string> AvailableFormats();
void Clear();
std::string ReadHTML();
gfx::Image ReadImage();
std::string ReadText();
void Write(const ClipboardData& data);
void WriteHTML(const std::string& markup);
void WriteImage(const gfx::Image& image);
void WriteText(const std::string& text);

}  // namespace lynxtron::api::clipboard

#endif  // LYNXTRON_SHELL_API_API_CLIPBOARD_H_
