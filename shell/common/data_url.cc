// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: based loosely on mozilla's nsDataChannel.cpp

#include "shell/common/data_url.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/simdutf/simdutf.h"
// #include "net/base/base64.h"
// #include "net/base/features.h"
// #include "net/base/mime_util.h"
// #include "net/http/http_response_headers.h"
// #include "net/http/http_util.h"
#include "url/gurl.h"

namespace net {

namespace {

// Determine if we are in the deprecated mode of whitespace removal
// Enterprise policies can enable this command line flag to force
// the old (non-standard compliant) behavior.
// bool HasRemoveWhitespaceCommandLineFlag() {
//   const base::CommandLine* command_line =
//       base::CommandLine::ForCurrentProcess();
//   if (!command_line) {
//     return false;
//   }
//   return command_line->HasSwitch(kRemoveWhitespaceForDataURLs);
// }
bool SimdutfBase64Decode(std::string_view input,
                         std::string* output,
                         base::Base64DecodePolicy policy) {
  // CHECK(base::FeatureList::IsEnabled(features::kSimdutfBase64Support));
  if (policy == base::Base64DecodePolicy::kStrict) {
    if (input.size() % 4 != 0) {
      // The input is not properly padded.
      return false;
    }
    if (std::ranges::any_of(input, [](char c) {
          return base::Contains(base::kInfraAsciiWhitespace, c);
        })) {
      return false;
    }
  }

  std::string decode_buf;
  decode_buf.resize(
      simdutf::maximal_binary_length_from_base64(input.data(), input.size()));
  simdutf::result r =
      simdutf::base64_to_binary(input.data(), input.size(), decode_buf.data());
  if (r.error != simdutf::error_code::SUCCESS) {
    return false;
  }
  // If this failed it would indicate we wrote OOB. It's possible for this to
  // be elided by the compiler, since writing OOB is UB.
  CHECK_LE(r.count, decode_buf.size());

  // Shrinks the buffer and makes it NUL-terminated.
  decode_buf.resize(r.count);

  *output = std::move(decode_buf);
  return true;
}

bool IsFurtherOptimizeParsingDataUrlsEnabled() {
  return false;
}

bool IsTokenChar(char c) {
  return !(c >= 0x7F || c <= 0x20 || c == '(' || c == ')' || c == '<' ||
           c == '>' || c == '@' || c == ',' || c == ';' || c == ':' ||
           c == '\\' || c == '"' || c == '/' || c == '[' || c == ']' ||
           c == '?' || c == '=' || c == '{' || c == '}');
}

// See RFC 7230 Sec 3.2.6 for the definition of |token|.
bool IsToken(std::string_view string) {
  if (string.empty()) {
    return false;
  }
  for (char c : string) {
    if (!IsTokenChar(c)) {
      return false;
    }
  }
  return true;
}

bool ParseMimeTypeWithoutParameter(std::string_view type_string,
                                   std::string* top_level_type,
                                   std::string* subtype) {
  std::vector<std::string_view> components = base::SplitStringPiece(
      type_string, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (components.size() != 2) {
    return false;
  }
  components[0] = TrimWhitespaceASCII(components[0], base::TRIM_LEADING);
  components[1] = TrimWhitespaceASCII(components[1], base::TRIM_TRAILING);
  if (!IsToken(components[0]) || !IsToken(components[1])) {
    return false;
  }

  if (top_level_type) {
    top_level_type->assign(std::string(components[0]));
  }

  if (subtype) {
    subtype->assign(std::string(components[1]));
  }

  return true;
}

}  // namespace

bool DataURL::Parse(const GURL& url,
                    std::string* mime_type,
                    std::string* charset,
                    std::string* data) {
  if (!url.is_valid() || !url.has_scheme()) {
    return false;
  }

  DCHECK(mime_type->empty());
  DCHECK(charset->empty());
  DCHECK(!data || data->empty());

  // Avoid copying the URL content which can be expensive for large URLs.
  std::string_view content = url.GetContentPiece();

  std::optional<std::pair<std::string_view, std::string_view>>
      media_type_and_body = base::SplitStringOnce(content, ',');
  if (!media_type_and_body) {
    return false;
  }

  std::vector<std::string_view> meta_data =
      base::SplitStringPiece(media_type_and_body->first, ";",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // These are moved to |mime_type| and |charset| on success.
  std::string mime_type_value;
  std::string charset_value;
  auto iter = meta_data.cbegin();
  if (iter != meta_data.cend()) {
    mime_type_value = base::ToLowerASCII(*iter);
    ++iter;
  }

  static constexpr std::string_view kBase64Tag("base64");
  static constexpr std::string_view kCharsetTag("charset=");

  bool base64_encoded = false;
  for (; iter != meta_data.cend(); ++iter) {
    if (!base64_encoded &&
        base::EqualsCaseInsensitiveASCII(*iter, kBase64Tag)) {
      base64_encoded = true;
    } else if (charset_value.empty() &&
               base::StartsWith(*iter, kCharsetTag,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      charset_value = std::string(iter->substr(kCharsetTag.size()));
      // The grammar for charset is not specially defined in RFC2045 and
      // RFC2397. It just needs to be a token.
      if (!IsToken(charset_value)) {
        return false;
      }
    }
  }

  if (mime_type_value.empty()) {
    // Fallback to the default if nothing specified in the mediatype part as
    // specified in RFC2045. As specified in RFC2397, we use |charset| even if
    // |mime_type| is empty.
    mime_type_value = "text/plain";
    if (charset_value.empty()) {
      charset_value = "US-ASCII";
    }
  } else if (!ParseMimeTypeWithoutParameter(mime_type_value, nullptr,
                                            nullptr)) {
    // Fallback to the default as recommended in RFC2045 when the mediatype
    // value is invalid. For this case, we don't respect |charset| but force it
    // set to "US-ASCII".
    mime_type_value = "text/plain";
    charset_value = "US-ASCII";
  }

  // The caller may not be interested in receiving the data.
  if (data) {
    std::string_view raw_body = media_type_and_body->second;

    // For base64, we may have url-escaped whitespace which is not part
    // of the data, and should be stripped. Otherwise, the escaped whitespace
    // could be part of the payload, so don't strip it.
    if (base64_encoded) {
      if (IsFurtherOptimizeParsingDataUrlsEnabled()) {
        // Based on https://fetch.spec.whatwg.org/#data-url-processor, we can
        // always use forgiving-base64 decode.
        // Forgiving-base64 decode consists of 2 passes: removing all ASCII
        // whitespace, then base64 decoding. For data URLs, it consists of 3
        // passes: percent-decoding, removing all ASCII whitespace, then
        // base64 decoding. To do this with as few passes as possible, we try
        // base64 decoding without any modifications in the "happy path". If
        // that fails, we percent-decode, then try the base64 decode again.
        if (!SimdutfBase64Decode(raw_body, data,
                                 base::Base64DecodePolicy::kForgiving)) {
          std::string unescaped_body =
              base::UnescapeBinaryURLComponent(raw_body);
          if (!SimdutfBase64Decode(unescaped_body, data,
                                   base::Base64DecodePolicy::kForgiving)) {
            return false;
          }
        }
      } else {
        // Since whitespace and invalid characters in input will always cause
        // `Base64Decode` to fail, just handle unescaping the URL on failure.
        // This is not much slower than scanning the URL for being well formed
        // first, even for input with whitespace.
        if (!SimdutfBase64Decode(raw_body, data,
                                 base::Base64DecodePolicy::kStrict)) {
          std::string unescaped_body =
              base::UnescapeBinaryURLComponent(raw_body);
          if (!SimdutfBase64Decode(unescaped_body, data,
                                   base::Base64DecodePolicy::kForgiving)) {
            return false;
          }
        }
      }
    }
  }

  *mime_type = std::move(mime_type_value);
  *charset = std::move(charset_value);
  return true;
}

}  // namespace net
