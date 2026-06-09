// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_impl.h"

#include <memory>
#include <string>

#include "platform/embedder/public/capi/lynx_generic_resource_fetcher_capi.h"
#include "platform/embedder/public/capi/lynx_memory_capi.h"
#include "shell/api/api_lynx_window.h"
#include "shell/api/lynx_view/module/lynx_emit_event.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/global_thread.h"
#include "shell/lynx/resource_fetcher/lynx_generic_resource_fetcher_factory.h"
#include "shell/lynx/resource_fetcher/lynx_protocol_resource_handler.h"
#include "v8.h"

namespace lynxtron {
namespace {

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
  GlobalThread::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::shared_ptr<lynx::pub::resource::LynxResourceRequest> request,
             std::shared_ptr<lynx::pub::resource::LynxResourceResponse>
                 response,
             base::WeakPtr<api::LynxWindow> lynx_window) {
            std::string url = std::string(request->GetUrl());
            v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
            v8::Locker locker(isolate);
            v8::Isolate::Scope isolate_scope(isolate);
            v8::HandleScope handle_scope(isolate);
            url = resource_fetcher::RewriteRequestUrl(isolate, request, url);
            if (resource_fetcher::TryHandleProtocolRequest(isolate, request,
                                                           response, url)) {
              return;
            }
            if (!lynx_window) {
              return;
            }

            auto node_emit_event = LynxEmitEvent::Create(
                isolate, [response](v8::Isolate* isolate,
                                    v8::Local<v8::Value> data_val) {
                  int status_code = resource_fetcher::kDefaultErrorCode;
                  if (!resource_fetcher::TrySetResponseFromResult(
                          isolate, data_val, response, &status_code)) {
                    CompleteWithError(response, status_code,
                                      "Invalid reply payload");
                  }
                });
            const std::string& resource_type =
                resource_fetcher::GetResourceTypeString(request->GetType());
            lynx_window->EmitWithoutEvent("-on-fetch-resource", node_emit_event,
                                          resource_type, url);
          },
          request, response, lynx_window_));
}

void LynxGenericResourceFetcherImpl::FetchResourcePath(
    std::shared_ptr<lynx::pub::resource::LynxResourceRequest> request,
    std::shared_ptr<lynx::pub::resource::LynxResourceResponse> response) {
  CompleteWithError(response, resource_fetcher::kDefaultErrorCode, "No cache");
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
