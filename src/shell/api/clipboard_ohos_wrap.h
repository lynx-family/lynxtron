// Copyright 2026 The Lynxtron Authors. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//
// Pure-C isolation layer over the OHOS pasteboard / UDMF / UDS NDK APIs.
//
// Why this exists: including <database/pasteboard/oh_pasteboard.h> from a
// C++ translation unit that also sees lynxtron's bundled Node.js headers
// pulls in <database/udmf/uds.h> → <multimedia/.../pixelmap_native.h> →
// <napi/native_api.h>, whose napi_* declarations collide with Node's napi.h.
// The official headers are therefore only ever included from this C file,
// which never sees any lynxtron/node header.  C++ consumers include only
// this header, which is free of OHOS types.
//
// Error code convention (all functions):
//   0     success.  For the read functions, success with no data yields
//         *out = NULL and return 0.
//   >0    OHOS PASTEBOARD_ERR_* code (201 = permission denied,
//         401 = invalid parameter, 12900000 = internal).
//   <0    local internal failure (allocation etc.).
//
// *out strings are malloc'd and owned by the caller (free() them).

#ifndef LYNXTRON_SHELL_API_CLIPBOARD_OHOS_WRAP_H_
#define LYNXTRON_SHELL_API_CLIPBOARD_OHOS_WRAP_H_

#ifdef __cplusplus
extern "C" {
#endif

// Read primary plain text (text/plain) from the system pasteboard.
int lynxtron_clipboard_read_text(char** out);

// Read primary HTML (text/html) from the system pasteboard.
int lynxtron_clipboard_read_html(char** out);

// Query availability of a plain-text entry (1 = present, 0 = absent).
int lynxtron_clipboard_has_text(int* out);

// Query availability of an HTML entry (1 = present, 0 = absent).
int lynxtron_clipboard_has_html(int* out);

// Query whether the pasteboard holds any data at all (1/0).
int lynxtron_clipboard_has_data(int* out);

// Write plain text and/or HTML in one UDMF record.  Either pointer may be
// NULL or empty; if both are, nothing is written and 0 is returned.
int lynxtron_clipboard_write(const char* plain, const char* html);

// Clear the system pasteboard.
int lynxtron_clipboard_clear(void);

#ifdef __cplusplus
}
#endif

#endif  // LYNXTRON_SHELL_API_CLIPBOARD_OHOS_WRAP_H_
