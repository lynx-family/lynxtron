// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/resource_fetcher/lynx_protocol_resource_handler.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "shell/api/api_protocol.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/global_thread.h"
#include "shell/lynx/resource_fetcher/lynx_resource_response_utils.h"
#include "v8.h"

namespace lynxtron::resource_fetcher {
namespace {

struct PendingProtocolRequest {
  PendingProtocolRequest(std::shared_ptr<LynxResourceResponse> response,
                         std::function<void()> fallback)
      : response(std::move(response)), fallback(std::move(fallback)) {}

  void RunFallback() {
    if (!fallback) {
      return;
    }
    auto run_fallback = std::move(fallback);
    fallback = nullptr;
    run_fallback();
  }

  std::shared_ptr<LynxResourceResponse> response;
  std::function<void()> fallback;
};

void OnProtocolPromiseResolved(
    std::shared_ptr<PendingProtocolRequest> pending_request,
    v8::Local<v8::Value> result) {
  if (!TrySetResponseFromResult(v8::Isolate::GetCurrent(), result,
                                pending_request->response)) {
    pending_request->RunFallback();
  }
}

void OnProtocolPromiseRejected(
    std::shared_ptr<PendingProtocolRequest> pending_request,
    v8::Local<v8::Value>) {
  pending_request->RunFallback();
}

void HandleProtocolPromise(
    v8::Isolate* isolate,
    v8::Local<v8::Promise> promise,
    const std::shared_ptr<LynxResourceResponse>& response,
    std::function<void()> fallback) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  auto pending_request =
      std::make_shared<PendingProtocolRequest>(response, std::move(fallback));
  v8::Local<v8::Value> resolve_value = gin::ConvertToV8(
      isolate, base::BindOnce(&OnProtocolPromiseResolved, pending_request));
  v8::Local<v8::Value> reject_value = gin::ConvertToV8(
      isolate, base::BindOnce(&OnProtocolPromiseRejected, pending_request));
  if (!resolve_value->IsFunction() || !reject_value->IsFunction()) {
    pending_request->RunFallback();
    return;
  }

  if (promise
          ->Then(context, resolve_value.As<v8::Function>(),
                 reject_value.As<v8::Function>())
          .IsEmpty()) {
    pending_request->RunFallback();
    return;
  }
}

}  // namespace

const char* GetResourceTypeString(lynx_resource_type_e type) {
  switch (type) {
    case kLynxResourceTypeGeneric:
      return "Generic";
    case kLynxResourceTypeImage:
      return "Image";
    case kLynxResourceTypeFont:
      return "Font";
    case kLynxResourceTypeLottie:
      return "Lottie";
    case kLynxResourceTypeVideo:
      return "Video";
    case kLynxResourceTypeSVG:
      return "SVG";
    case kLynxResourceTypeTemplate:
      return "Template";
    case kLynxResourceTypeLynxCoreJS:
      return "LynxCoreJS";
    case kLynxResourceTypeLazyBundle:
      return "LazyBundle";
    default:
      return "Unknown";
  }
}

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

void HandleProtocolRequest(
    const std::shared_ptr<LynxResourceRequest>& request,
    const std::shared_ptr<LynxResourceResponse>& response,
    const std::string& url,
    std::function<void()> fallback) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::Locker locker(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MicrotasksScope microtasks_scope(context,
                                       v8::MicrotasksScope::kRunMicrotasks);

  v8::Local<v8::Value> result;
  if (!api::protocol::InvokeHandler(isolate, url,
                                    GetResourceTypeString(request->GetType()))
           .ToLocal(&result)) {
    if (fallback) {
      fallback();
    }
    return;
  }
  if (TrySetResponseFromResult(isolate, result, response, nullptr)) {
    return;
  }
  if (result->IsPromise()) {
    HandleProtocolPromise(isolate, result.As<v8::Promise>(), response,
                          std::move(fallback));
    return;
  }
  if (fallback) {
    fallback();
  }
}

}  // namespace lynxtron::resource_fetcher
