// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/api_clipboard.h"

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/gfx/image/image.h"

namespace lynxtron::api::clipboard {

namespace {

NSString* HTMLType() {
  if (@available(macOS 10.13, *)) {
    return NSPasteboardTypeHTML;
  }
  return @"public.html";
}

}  // namespace

std::vector<std::string> AvailableFormats() {
  std::vector<std::string> result;
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  if ([pb availableTypeFromArray:@[ NSPasteboardTypeString ]] != nil) {
    result.emplace_back("text/plain");
  }
  if ([pb availableTypeFromArray:@[ HTMLType() ]] != nil) {
    result.emplace_back("text/html");
  }
  if ([pb availableTypeFromArray:@[ NSPasteboardTypePNG ]] != nil) {
    result.emplace_back("image/png");
  }
  if ([pb availableTypeFromArray:@[ NSPasteboardTypeTIFF ]] != nil) {
    result.emplace_back("image/tiff");
  }
  return result;
}

void Clear() {
  [[NSPasteboard generalPasteboard] clearContents];
}

std::string ReadHTML() {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSString* s = [pb stringForType:HTMLType()];
  if (!s) {
    return std::string();
  }
  return base::SysNSStringToUTF8(s);
}

gfx::Image ReadImage() {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSData* png_data = [pb dataForType:NSPasteboardTypePNG];
  if (png_data != nil) {
    return gfx::Image::CreateFrom1xPNGBytes(base::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(png_data.bytes),
        static_cast<size_t>(png_data.length)));
  }
  NSImage* image = [[NSImage alloc] initWithPasteboard:pb];
  if (image == nil) {
    return gfx::Image();
  }
  NSData* tiff_data = [image TIFFRepresentation];
  if (tiff_data == nil) {
    return gfx::Image();
  }
  NSBitmapImageRep* bitmap_rep = [NSBitmapImageRep imageRepWithData:tiff_data];
  NSData* converted_png =
      [bitmap_rep representationUsingType:NSBitmapImageFileTypePNG
                               properties:@{}];
  if (converted_png == nil) {
    return gfx::Image();
  }
  return gfx::Image::CreateFrom1xPNGBytes(base::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(converted_png.bytes),
      static_cast<size_t>(converted_png.length)));
}

std::string ReadText() {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSString* s = [pb stringForType:NSPasteboardTypeString];
  if (!s) {
    return std::string();
  }
  return base::SysNSStringToUTF8(s);
}

void Write(const ClipboardData& data) {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb clearContents];
  NSPasteboardItem* item = [[NSPasteboardItem alloc] init];
  if (data.text.has_value()) {
    [item setString:base::SysUTF8ToNSString(*data.text)
            forType:NSPasteboardTypeString];
  }
  if (data.html.has_value()) {
    [item setString:base::SysUTF8ToNSString(*data.html) forType:HTMLType()];
  }
  if (data.image.has_value() && !data.image->IsEmpty()) {
    const scoped_refptr<base::RefCountedMemory> png =
        data.image->As1xPNGBytes();
    const base::span<const uint8_t> bytes = *png;
    if (!bytes.empty()) {
      NSData* image_data = [NSData dataWithBytes:bytes.data()
                                          length:bytes.size()];
      [item setData:image_data forType:NSPasteboardTypePNG];
      NSImage* ns_image = [[NSImage alloc] initWithData:image_data];
      if (ns_image) {
        NSData* tiff_data = [ns_image TIFFRepresentation];
        if (tiff_data) {
          [item setData:tiff_data forType:NSPasteboardTypeTIFF];
        }
      }
    }
  }
  if ([[item types] count] > 0) {
    [pb writeObjects:@[ item ]];
  }
}

void WriteHTML(const std::string& markup) {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb clearContents];
  NSPasteboardItem* item = [[NSPasteboardItem alloc] init];
  [item setString:base::SysUTF8ToNSString(markup) forType:HTMLType()];
  [pb writeObjects:@[ item ]];
}

void WriteImage(const gfx::Image& image) {
  if (image.IsEmpty()) {
    return;
  }
  ClipboardData data;
  data.image = image;
  Write(data);
}

void WriteText(const std::string& text) {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb clearContents];
  [pb setString:base::SysUTF8ToNSString(text) forType:NSPasteboardTypeString];
}

}  // namespace lynxtron::api::clipboard
