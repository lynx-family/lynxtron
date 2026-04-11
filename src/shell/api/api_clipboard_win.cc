// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_clipboard.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_hglobal.h"
#include "shell/ui/skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace lynxtron::api::clipboard {

namespace {

UINT HtmlFormat() {
  static UINT k = RegisterClipboardFormatW(L"HTML Format");
  return k;
}

bool Open() {
  return !!OpenClipboard(nullptr);
}

void Close() {
  CloseClipboard();
}

std::string BuildClipboardHtml(const std::string& markup) {
  static constexpr char kHtmlPrefix[] =
      "Version:0.9\r\n"
      "StartHTML:%010u\r\n"
      "EndHTML:%010u\r\n"
      "StartFragment:%010u\r\n"
      "EndFragment:%010u\r\n";
  static constexpr char kStart[] = "<html><body><!--StartFragment-->";
  static constexpr char kEnd[] = "<!--EndFragment--></body></html>";

  char header[256];
  std::snprintf(header, sizeof(header), kHtmlPrefix, 0u, 0u, 0u, 0u);
  const std::string header_string(header);
  const unsigned int start_html =
      base::checked_cast<unsigned int>(header_string.size());
  const unsigned int start_fragment =
      start_html + base::checked_cast<unsigned int>(sizeof(kStart) - 1);
  const unsigned int end_fragment =
      start_fragment + base::checked_cast<unsigned int>(markup.size());
  const unsigned int end_html =
      end_fragment + base::checked_cast<unsigned int>(sizeof(kEnd) - 1);

  std::snprintf(header, sizeof(header), kHtmlPrefix, start_html, end_html,
                start_fragment, end_fragment);
  return std::string(header) + kStart + markup + kEnd;
}

std::string ReadClipboardText(HANDLE handle) {
  base::win::ScopedHGlobal<wchar_t*> data(handle);
  if (!data.get()) {
    return {};
  }
  return base::WideToUTF8(data.get());
}

std::string ReadClipboardBytes(HANDLE handle) {
  base::win::ScopedHGlobal<char*> data(handle);
  if (!data.get()) {
    return {};
  }
  const size_t size = GlobalSize(handle);
  if (size == 0) {
    return {};
  }
  size_t actual_size = size;
  if (data.get()[actual_size - 1] == '\0') {
    --actual_size;
  }
  return std::string(data.get(), actual_size);
}

std::string ExtractHtmlFragment(const std::string& html) {
  size_t start_pos = html.find("StartFragment:");
  size_t end_pos = html.find("EndFragment:");
  if (start_pos == std::string::npos || end_pos == std::string::npos) {
    return html;
  }

  const size_t start_line = start_pos + strlen("StartFragment:");
  const size_t end_line = end_pos + strlen("EndFragment:");
  const unsigned int start = std::strtoul(
      std::string(html.substr(start_line, 10)).c_str(), nullptr, 10);
  const unsigned int end =
      std::strtoul(std::string(html.substr(end_line, 10)).c_str(), nullptr, 10);
  if (start >= end || end > html.size()) {
    return html;
  }
  return html.substr(start, end - start);
}

gfx::Image ReadDib(UINT format) {
  HANDLE handle = GetClipboardData(format);
  if (!handle) {
    return gfx::Image();
  }

  base::win::ScopedHGlobal<BITMAPINFOHEADER*> dib(handle);
  if (!dib.get()) {
    return gfx::Image();
  }

  const BITMAPINFOHEADER* header = dib.get();
  if (header->biBitCount != 32 ||
      (header->biCompression != BI_RGB &&
       header->biCompression != BI_BITFIELDS) ||
      header->biWidth <= 0 || header->biHeight == 0) {
    return gfx::Image();
  }

  size_t pixel_offset = header->biSize;
  if (header->biSize == sizeof(BITMAPINFOHEADER) &&
      header->biCompression == BI_BITFIELDS) {
    pixel_offset += 3 * sizeof(uint32_t);
  }

  const uint8_t* const bytes = reinterpret_cast<const uint8_t*>(header);
  const int width = header->biWidth;
  const int height = std::abs(header->biHeight);
  const bool bottom_up = header->biHeight > 0;
  const size_t row_bytes = static_cast<size_t>(width) * 4;
  const size_t total_bytes =
      pixel_offset + row_bytes * static_cast<size_t>(height);
  if (GlobalSize(handle) < total_bytes) {
    return gfx::Image();
  }

  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, false);
  const uint8_t* src_pixels = bytes + pixel_offset;
  for (int y = 0; y < height; ++y) {
    const int src_row = bottom_up ? (height - 1 - y) : y;
    memcpy(bitmap.getAddr32(0, y),
           src_pixels + row_bytes * static_cast<size_t>(src_row), row_bytes);
  }
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

}  // namespace

std::vector<std::string> AvailableFormats() {
  std::vector<std::string> result;
  if (!Open()) {
    return result;
  }
  if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    result.emplace_back("text/plain");
  }
  if (IsClipboardFormatAvailable(HtmlFormat())) {
    result.emplace_back("text/html");
  }
  if (IsClipboardFormatAvailable(CF_DIBV5) ||
      IsClipboardFormatAvailable(CF_DIB) ||
      IsClipboardFormatAvailable(CF_BITMAP)) {
    result.emplace_back("image/png");
  }
  Close();
  return result;
}

void Clear() {
  if (!Open()) {
    return;
  }
  EmptyClipboard();
  Close();
}

std::string ReadHTML() {
  std::string out;
  if (!Open()) {
    return out;
  }
  HANDLE h = GetClipboardData(HtmlFormat());
  if (!h) {
    Close();
    return out;
  }
  out = ExtractHtmlFragment(ReadClipboardBytes(h));
  Close();
  return out;
}

gfx::Image ReadBitmap() {
  HBITMAP hbitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
  if (!hbitmap) {
    return gfx::Image();
  }
  BITMAP bmp = {};
  if (!GetObject(hbitmap, sizeof(bmp), &bmp) || bmp.bmWidth <= 0 ||
      bmp.bmHeight <= 0) {
    return gfx::Image();
  }
  const int width = bmp.bmWidth;
  const int height = bmp.bmHeight;
  BITMAPINFOHEADER bi = {};
  bi.biSize = sizeof(BITMAPINFOHEADER);
  bi.biWidth = width;
  bi.biHeight = -height;
  bi.biPlanes = 1;
  bi.biBitCount = 32;
  bi.biCompression = BI_RGB;
  HDC hdc = GetDC(nullptr);
  if (!hdc) {
    return gfx::Image();
  }
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, false);
  const bool ok =
      GetDIBits(hdc, hbitmap, 0, static_cast<UINT>(height), bitmap.getPixels(),
                reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS) == height;
  ReleaseDC(nullptr, hdc);
  if (!ok) {
    return gfx::Image();
  }
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

gfx::Image ReadImage() {
  gfx::Image image;
  if (!Open()) {
    return image;
  }
  if (IsClipboardFormatAvailable(CF_DIBV5)) {
    image = ReadDib(CF_DIBV5);
  } else if (IsClipboardFormatAvailable(CF_DIB)) {
    image = ReadDib(CF_DIB);
  } else if (IsClipboardFormatAvailable(CF_BITMAP)) {
    image = ReadBitmap();
  }
  Close();
  return image;
}

std::string ReadText() {
  std::string out;
  if (!Open()) {
    return out;
  }
  HANDLE h = GetClipboardData(CF_UNICODETEXT);
  if (h) {
    out = ReadClipboardText(h);
  }
  Close();
  return out;
}

void Write(const ClipboardData& data) {
  if (!Open()) {
    return;
  }
  EmptyClipboard();
  if (data.text.has_value()) {
    const std::u16string w = base::UTF8ToUTF16(*data.text);
    size_t size_bytes = (w.size() + 1) * sizeof(wchar_t);
    HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE, size_bytes);
    if (hglb) {
      base::win::ScopedHGlobal<wchar_t*> dst(hglb);
      memcpy(dst.get(), w.c_str(), size_bytes);
      SetClipboardData(CF_UNICODETEXT, hglb);
      dst.release();  // Ownership transferred to clipboard
    }
  }
  if (data.html.has_value()) {
    std::string payload = BuildClipboardHtml(*data.html);
    HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE, payload.size() + 1);
    if (hglb) {
      base::win::ScopedHGlobal<char*> dst(hglb);
      memcpy(dst.get(), payload.c_str(), payload.size() + 1);
      SetClipboardData(HtmlFormat(), hglb);
      dst.release();
    }
  }
  if (data.image.has_value() && !data.image->IsEmpty()) {
    const SkBitmap bitmap =
        *data.image->AsImageSkia().GetRepresentation(1.0f).bitmap();
    HGLOBAL dibv5 = skia::CreateDIBV5ImageDataFromN32SkBitmap(bitmap);
    if (dibv5) {
      SetClipboardData(CF_DIBV5, dibv5);
    }
  }
  Close();
}

void WriteHTML(const std::string& markup) {
  ClipboardData d;
  d.html = markup;
  Write(d);
}

void WriteImage(const gfx::Image& image) {
  ClipboardData d;
  d.image = image;
  Write(d);
}

void WriteText(const std::string& text) {
  ClipboardData d;
  d.text = text;
  Write(d);
}

}  // namespace lynxtron::api::clipboard
