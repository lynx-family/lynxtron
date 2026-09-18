// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
//
// HarmonyOS clipboard implementation using native OH_Pasteboard C API.
//
// NOTE: All OHOS NDK calls live in clipboard_ohos_wrap.c, a pure-C
// translation unit that includes the official SDK headers.  Including
// <database/pasteboard/oh_pasteboard.h> from C++ would pull in
// <database/udmf/uds.h> → <multimedia/.../pixelmap_native.h> →
// <napi/native_api.h>, whose napi_* declarations conflict with lynxtron's
// bundled Node.js napi headers.  This file only sees the opaque wrap
// interface, which links against libpasteboard.so + libudmf.so via the
// wrap TU.

#include "shell/api/api_clipboard.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "shell/api/clipboard_ohos_wrap.h"

namespace lynxtron::api::clipboard {

namespace {

// Log a wrap-layer return code.  >0 is an OHOS PASTEBOARD_ERR_* code
// (201 = permission denied, 401 = invalid parameter, 12900000 = internal);
// <0 is a local failure.  Returns the code for callers that need it.
int LogIfError(int ret, const char* what) {
  if (ret > 0) {
    if (ret == 201) {
      LOG(WARNING) << "clipboard: " << what
                   << " requires ohos.permission.READ_PASTEBOARD";
    } else {
      LOG(ERROR) << "clipboard: " << what << " failed, code=" << ret;
    }
  } else if (ret < 0) {
    LOG(ERROR) << "clipboard: " << what << " failed (internal error)";
  }
  return ret;
}

// Owns a malloc'd string returned by the wrap layer.
struct WrapStringDeleter {
  void operator()(char* p) const { free(p); }
};
using ScopedWrapString = std::unique_ptr<char, WrapStringDeleter>;

}  // namespace

std::vector<std::string> AvailableFormats() {
  std::vector<std::string> result;
  int has = 0;
  if (lynxtron_clipboard_has_data(&has) != 0 || !has)
    return result;
  int is_text = 0;
  int is_html = 0;
  if (lynxtron_clipboard_has_text(&is_text) == 0 && is_text)
    result.emplace_back("text/plain");
  if (lynxtron_clipboard_has_html(&is_html) == 0 && is_html)
    result.emplace_back("text/html");
  return result;
}

void Clear() {
  LogIfError(lynxtron_clipboard_clear(), "ClearData");
}

std::string ReadHTML() {
  char* out = nullptr;
  int ret = lynxtron_clipboard_read_html(&out);
  ScopedWrapString guard(out);
  if (ret != 0)
    return {};
  return out ? std::string(out) : std::string();
}

gfx::Image ReadImage() {
  // M2: OH_UdsPixelMap → gfx::Image conversion
  return {};
}

std::string ReadText() {
  char* out = nullptr;
  int ret = lynxtron_clipboard_read_text(&out);
  ScopedWrapString guard(out);
  if (ret != 0)
    return {};
  return out ? std::string(out) : std::string();
}

void Write(const ClipboardData& data) {
  const char* plain = nullptr;
  const char* html = nullptr;
  std::string text_storage;
  std::string html_storage;
  if (data.text.has_value()) {
    text_storage = *data.text;
    plain = text_storage.c_str();
  }
  if (data.html.has_value()) {
    html_storage = *data.html;
    html = html_storage.c_str();
  }
  LogIfError(lynxtron_clipboard_write(plain, html), "SetData");
}

void WriteHTML(const std::string& markup) {
  LogIfError(lynxtron_clipboard_write(nullptr, markup.c_str()), "SetData");
}

void WriteImage(const gfx::Image&) {
  // M2
}

void WriteText(const std::string& text) {
  LogIfError(lynxtron_clipboard_write(text.c_str(), nullptr), "SetData");
}

}  // namespace lynxtron::api::clipboard
