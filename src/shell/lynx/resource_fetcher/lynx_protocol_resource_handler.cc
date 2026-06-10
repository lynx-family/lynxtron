// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/resource_fetcher/lynx_protocol_resource_handler.h"

#include "shell/api/api_protocol.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/global_thread.h"
#include "v8.h"

namespace lynxtron::resource_fetcher {

std::string RewriteRequestUrl(const std::string& url) {
  if (!GlobalThread::CurrentlyOn(GlobalThread::UI)) {
    return url;
  }

  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::Locker locker(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> result;
  if (!api::protocol::InvokeRequestRewriter(isolate, url).ToLocal(&result) ||
      result.IsEmpty() || !result->IsString()) {
    return url;
  }

  v8::String::Utf8Value rewritten_url(isolate, result);
  if (*rewritten_url == nullptr) {
    return url;
  }
  return std::string(*rewritten_url, rewritten_url.length());
}

}  // namespace lynxtron::resource_fetcher
