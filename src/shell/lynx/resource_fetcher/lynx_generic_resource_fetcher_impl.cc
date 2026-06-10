// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_impl.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "node_buffer.h"
#include "platform/embedder/public/capi/lynx_generic_resource_fetcher_capi.h"
#include "platform/embedder/public/capi/lynx_memory_capi.h"
#include "shell/api/api_lynx_window.h"
#include "shell/api/lynx_view/module/lynx_emit_event.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/global_thread.h"
#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_factory.h"
#include "shell/lynx/resource_fetcher/lynx_protocol_resource_handler.h"
#include "v8.h"

namespace lynxtron {
namespace {
constexpr int kDefaultErrorCode = 1;

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

void CompleteWithError(
    const std::shared_ptr<lynx::pub::resource::LynxResourceResponse>& response,
    int code,
    const char* message) {
  if (!response) {
    return;
  }
  response->SetCode(code);
  if (message) {
    response->SetErrorMessage(message);
  }
  response->Complete();
}

void FreeVector(uint8_t*, size_t, void* opaque) {
  delete static_cast<std::vector<uint8_t>*>(opaque);
}

bool SetResponseDataFromV8Value(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const std::shared_ptr<lynx::pub::resource::LynxResourceResponse>&
        response) {
  if (value.IsEmpty() || value->IsNullOrUndefined() ||
      !node::Buffer::HasInstance(value)) {
    return false;
  }

  const char* buf_data = node::Buffer::Data(value);
  const size_t buf_len = node::Buffer::Length(value);

  auto* vec = new std::vector<uint8_t>(buf_len);
  std::memcpy(vec->data(), buf_data, buf_len);
  response->SetData(vec->data(), buf_len, &FreeVector, vec);
  response->Complete();
  return true;
}
}  // namespace

LynxGenericResourceFetcherImpl::LynxGenericResourceFetcherImpl(
    base::WeakPtr<api::LynxWindow> lynx_window)
    : lynx_window_(lynx_window) {}

std::string LynxGenericResourceFetcherImpl::InterceptUrl(
    const std::string& url) const {
  if (!lynx_window_) {
    return url;
  }
  return lynx_window_->ResolveResourceUrl(url);
}

void LynxGenericResourceFetcherImpl::FetchResource(
    std::shared_ptr<lynx::pub::resource::LynxResourceRequest> request,
    std::shared_ptr<lynx::pub::resource::LynxResourceResponse> response) {
  if (!request || !response || !request->GetUrl()) {
    return;
  }
  if (!lynx_window_) {
    return;
  }
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::shared_ptr<lynx::pub::resource::LynxResourceRequest> request,
             std::shared_ptr<lynx::pub::resource::LynxResourceResponse>
                 response,
             base::WeakPtr<api::LynxWindow> lynx_window) {
            if (!lynx_window) {
              return;
            }
            std::string url =
                resource_fetcher::RewriteRequestUrl(request->GetUrl());
            v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
            v8::Locker locker(isolate);
            v8::Isolate::Scope isolate_scope(isolate);
            v8::HandleScope handle_scope(isolate);
            auto node_emit_event = LynxEmitEvent::Create(
                isolate, [response](v8::Isolate* isolate,
                                    v8::Local<v8::Value> data_val) {
                  if (data_val.IsEmpty() || !data_val->IsObject()) {
                    CompleteWithError(response, kDefaultErrorCode,
                                      "Invalid reply payload");
                    return;
                  }
                  gin_helper::Dictionary dict(isolate,
                                              data_val.As<v8::Object>());
                  int status_code = kDefaultErrorCode;
                  dict.Get("statusCode", &status_code);
                  response->SetCode(status_code);
                  v8::Local<v8::Value> buf_val;
                  if (!dict.Get("data", &buf_val)) {
                    CompleteWithError(response, status_code, "No data field");
                    return;
                  }
                  if (!SetResponseDataFromV8Value(isolate, buf_val, response)) {
                    CompleteWithError(response, status_code,
                                      "Unsupported data type");
                    return;
                  }
                  response->Complete();
                });
            const std::string& resource_type =
                GetResourceTypeString(request->GetType());
            lynx_window->EmitWithoutEvent("-on-fetch-resource", node_emit_event,
                                          resource_type, url);
          },
          request, response, lynx_window_));
}

void LynxGenericResourceFetcherImpl::FetchResourcePath(
    std::shared_ptr<lynx::pub::resource::LynxResourceRequest> request,
    std::shared_ptr<lynx::pub::resource::LynxResourceResponse> response) {
  CompleteWithError(response, kDefaultErrorCode, "No cache");
}

std::shared_ptr<lynx::pub::LynxGenericResourceFetcher>
LynxGenericResourceFetcherFactory::Create(
    base::WeakPtr<api::LynxWindow> lynx_window) {
  auto fetcher = std::make_shared<LynxGenericResourceFetcherImpl>(lynx_window);
  fetcher->InitIfNeeded();
  lynx_generic_resource_fetcher_bind_intercept_func(
      fetcher->Impl(),
      [](const char* url, bool /*should_decode*/, void* user_data) -> char* {
        auto* weak_ptr = reinterpret_cast<
            std::weak_ptr<lynx::pub::LynxGenericResourceFetcher>*>(user_data);
        auto shared_fetcher = weak_ptr ? weak_ptr->lock() : nullptr;
        auto impl = std::static_pointer_cast<LynxGenericResourceFetcherImpl>(
            shared_fetcher);
        if (!impl || !url) {
          return lynx_strdup(url ? url : "");
        }
        std::string intercepted = impl->InterceptUrl(url);
        return lynx_strdup(intercepted.c_str());
      });
  return fetcher;
}

}  // namespace lynxtron
