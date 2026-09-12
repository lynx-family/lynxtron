// Copyright 2026 The Lynxtron Authors. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//
// Pure-C implementation of the OHOS pasteboard wrapper (clipboard_ohos_wrap.h).
//
// This translation unit is the ONLY place in lynxtron that includes the
// official OHOS NDK headers (oh_pasteboard.h / udmf.h / uds.h).  It never
// includes any lynxtron or Node.js header, so the napi_* symbol collision
// that blocks header inclusion from C++ cannot happen here.
//
// All resources are manually released on every path (no RAII in C).

#include "shell/api/clipboard_ohos_wrap.h"

#include <database/pasteboard/oh_pasteboard.h>
#include <database/pasteboard/oh_pasteboard_err_code.h>
#include <database/udmf/udmf.h>
#include <database/udmf/uds.h>

#include <stdlib.h>
#include <string.h>

static const char* kMimePlain = "text/plain";
static const char* kMimeHtml = "text/html";

// strdup is not visible under the default feature-test macros of the OHOS
// (musl) libc, so implement it directly.  Caller owns the result.
static char* dup_str(const char* s) {
  size_t n = strlen(s);
  char* p = (char*)malloc(n + 1);
  if (p)
    memcpy(p, s, n + 1);
  return p;
}

// Build a UDMF data container holding plain text and/or HTML in one record.
// Either input may be NULL/empty (skipped).  Returns NULL on failure, having
// released every intermediate object.  Caller owns the returned OH_UdmfData*.
static OH_UdmfData* build_udmf(const char* plain, const char* html) {
  OH_UdmfRecord* rec = OH_UdmfRecord_Create();
  if (!rec)
    return NULL;

  if (plain && plain[0]) {
    OH_UdsPlainText* pt = OH_UdsPlainText_Create();
    if (!pt)
      goto fail;
    if (OH_UdsPlainText_SetContent(pt, plain) != 0) {
      OH_UdsPlainText_Destroy(pt);
      goto fail;
    }
    if (OH_UdmfRecord_AddPlainText(rec, pt) != 0) {
      OH_UdsPlainText_Destroy(pt);
      goto fail;
    }
    // The record consumes the payload; the object itself is ours to free.
    OH_UdsPlainText_Destroy(pt);
  }

  if (html && html[0]) {
    OH_UdsHtml* h = OH_UdsHtml_Create();
    if (!h)
      goto fail;
    if (OH_UdsHtml_SetContent(h, html) != 0) {
      OH_UdsHtml_Destroy(h);
      goto fail;
    }
    if (plain && plain[0]) {
      if (OH_UdsHtml_SetPlainContent(h, plain) != 0) {
        OH_UdsHtml_Destroy(h);
        goto fail;
      }
    }
    if (OH_UdmfRecord_AddHtml(rec, h) != 0) {
      OH_UdsHtml_Destroy(h);
      goto fail;
    }
    OH_UdsHtml_Destroy(h);
  }

  OH_UdmfData* data = OH_UdmfData_Create();
  if (!data)
    goto fail;
  if (OH_UdmfData_AddRecord(data, rec) != 0) {
    OH_UdmfData_Destroy(data);
    goto fail;
  }
  OH_UdmfRecord_Destroy(rec);
  return data;

fail:
  OH_UdmfRecord_Destroy(rec);
  return NULL;
}

// Push a UDMF data container to the system pasteboard, then destroy it.
// Returns 0 on success, the OHOS error code on failure.
static int commit(OH_UdmfData* data) {
  if (!data)
    return -1;
  OH_Pasteboard* pb = OH_Pasteboard_Create();
  if (!pb) {
    OH_UdmfData_Destroy(data);
    return -1;
  }
  int ret = OH_Pasteboard_SetData(pb, data);
  OH_Pasteboard_Destroy(pb);
  OH_UdmfData_Destroy(data);
  return ret;
}

// Fetch the primary UDMF data from the pasteboard.
// Returns an owned OH_UdmfData* or NULL (check *status for the cause).
static OH_UdmfData* fetch(int* status) {
  OH_Pasteboard* pb = OH_Pasteboard_Create();
  if (!pb) {
    *status = -1;
    return NULL;
  }
  int has = OH_Pasteboard_HasData(pb);
  if (!has) {
    OH_Pasteboard_Destroy(pb);
    *status = 0;
    return NULL;
  }
  OH_UdmfData* data = OH_Pasteboard_GetData(pb, status);
  OH_Pasteboard_Destroy(pb);
  return data;
}

// Pull the primary plain-text record out of the pasteboard.
// Success with no text yields *out = NULL and return 0.
static int read_text_impl(char** out) {
  *out = NULL;
  int status = 0;
  OH_UdmfData* data = fetch(&status);
  if (!data)
    return status;
  OH_UdsPlainText* pt = OH_UdsPlainText_Create();
  if (!pt) {
    OH_UdmfData_Destroy(data);
    return -1;
  }
  int r = OH_UdmfData_GetPrimaryPlainText(data, pt);
  OH_UdmfData_Destroy(data);
  if (r != 0) {
    OH_UdsPlainText_Destroy(pt);
    return r;
  }
  const char* c = OH_UdsPlainText_GetContent(pt);
  OH_UdsPlainText_Destroy(pt);
  if (c) {
    *out = dup_str(c);
    if (!*out)
      return -1;
  }
  return 0;
}

// Pull the primary HTML record out of the pasteboard.
// Success with no HTML yields *out = NULL and return 0.
static int read_html_impl(char** out) {
  *out = NULL;
  int status = 0;
  OH_UdmfData* data = fetch(&status);
  if (!data)
    return status;
  OH_UdsHtml* h = OH_UdsHtml_Create();
  if (!h) {
    OH_UdmfData_Destroy(data);
    return -1;
  }
  int r = OH_UdmfData_GetPrimaryHtml(data, h);
  OH_UdmfData_Destroy(data);
  if (r != 0) {
    OH_UdsHtml_Destroy(h);
    return r;
  }
  const char* c = OH_UdsHtml_GetContent(h);
  OH_UdsHtml_Destroy(h);
  if (c) {
    *out = dup_str(c);
    if (!*out)
      return -1;
  }
  return 0;
}

int lynxtron_clipboard_read_text(char** out) {
  return read_text_impl(out);
}

int lynxtron_clipboard_read_html(char** out) {
  return read_html_impl(out);
}

int lynxtron_clipboard_has_text(int* out) {
  *out = 0;
  OH_Pasteboard* pb = OH_Pasteboard_Create();
  if (!pb)
    return -1;
  *out = OH_Pasteboard_HasType(pb, kMimePlain) ? 1 : 0;
  OH_Pasteboard_Destroy(pb);
  return 0;
}

int lynxtron_clipboard_has_html(int* out) {
  *out = 0;
  OH_Pasteboard* pb = OH_Pasteboard_Create();
  if (!pb)
    return -1;
  *out = OH_Pasteboard_HasType(pb, kMimeHtml) ? 1 : 0;
  OH_Pasteboard_Destroy(pb);
  return 0;
}

int lynxtron_clipboard_has_data(int* out) {
  *out = 0;
  OH_Pasteboard* pb = OH_Pasteboard_Create();
  if (!pb)
    return -1;
  *out = OH_Pasteboard_HasData(pb) ? 1 : 0;
  OH_Pasteboard_Destroy(pb);
  return 0;
}

int lynxtron_clipboard_write(const char* plain, const char* html) {
  if ((!plain || !plain[0]) && (!html || !html[0]))
    return 0;  // nothing to write is not an error
  return commit(build_udmf(plain, html));
}

int lynxtron_clipboard_clear(void) {
  OH_Pasteboard* pb = OH_Pasteboard_Create();
  if (!pb)
    return -1;
  int ret = OH_Pasteboard_ClearData(pb);
  OH_Pasteboard_Destroy(pb);
  return ret;
}
