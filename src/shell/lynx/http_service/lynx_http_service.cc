// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/http_service/lynx_http_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "node_buffer.h"
#include "platform/embedder/public/lynx_http_service.h"
#include "platform/embedder/public/lynx_service_center.h"
#include "shell/app/javascript_environment.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/global_thread.h"
#include "shell/common/node_util.h"
#include "shell/lynx/http_service/lynx_http_service_binding.h"
#include "v8.h"

namespace lynxtron {
namespace {

using LynxHttpRequest = lynx::pub::LynxHttpRequest;
using LynxHttpResponse = lynx::pub::LynxHttpResponse;

struct HttpRequestData {
  std::string url;
  std::string method;
  std::map<std::string, std::string> headers;
  std::vector<uint8_t> body;
};

struct PendingHttpResponse {
  explicit PendingHttpResponse(std::shared_ptr<LynxHttpResponse> response)
      : response(std::move(response)) {}

  std::shared_ptr<LynxHttpResponse> response;
  bool replied = false;
};

void CompleteWithError(const std::shared_ptr<LynxHttpResponse>& response,
                       const std::string& message) {
  response->SetStatusCode(-1);
  response->SetStatusText(message.c_str());
  response->Complete();
}

bool TrySetResponseFromResult(
    v8::Isolate* isolate,
    v8::Local<v8::Value> result,
    const std::shared_ptr<LynxHttpResponse>& response) {
  if (result.IsEmpty() || !result->IsObject()) {
    return false;
  }

  gin_helper::Dictionary dict(isolate, result.As<v8::Object>());
  int status_code = -1;
  if (!dict.Get("statusCode", &status_code)) {
    return false;
  }

  std::string status_text;
  dict.Get("statusText", &status_text);

  std::map<std::string, std::string> headers;
  dict.Get("headers", &headers);

  v8::Local<v8::Value> data;
  if (!dict.Get("data", &data) || !node::Buffer::HasInstance(data)) {
    return false;
  }

  response->SetStatusCode(status_code);
  response->SetStatusText(status_text.c_str());
  for (const auto& [name, value] : headers) {
    response->AddHeader(name, value);
  }

  auto* body = reinterpret_cast<uint8_t*>(node::Buffer::Data(data));
  response->SetBody(body, node::Buffer::Length(data));
  response->Complete();
  return true;
}

void OnHttpRequestReply(std::shared_ptr<PendingHttpResponse> pending,
                        v8::Local<v8::Value> result) {
  pending->replied = true;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!TrySetResponseFromResult(isolate, result, pending->response)) {
    CompleteWithError(pending->response, "Invalid HTTP response payload");
  }
}

void RequestOnUIThread(HttpRequestData request,
                       std::shared_ptr<LynxHttpResponse> response) {
  DCHECK_CURRENTLY_ON(GlobalThread::UI);

  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::Locker locker(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  gin_helper::Dictionary request_data =
      gin_helper::Dictionary::CreateEmpty(isolate);
  v8::Local<v8::Object> body;
  if (!Buffer::Copy(isolate, base::span<const uint8_t>(request.body))
           .ToLocal(&body) ||
      !request_data.Set("url", request.url) ||
      !request_data.Set("method", request.method) ||
      !request_data.Set("headers", request.headers) ||
      !request_data.Set("body", body)) {
    CompleteWithError(response, "Failed to serialize HTTP request");
    return;
  }

  auto pending = std::make_shared<PendingHttpResponse>(std::move(response));
  v8::Local<v8::Value> reply =
      gin::ConvertToV8(isolate, base::BindOnce(&OnHttpRequestReply, pending));
  if (!reply->IsFunction() ||
      !InvokeLynxHttpRequestHandler(isolate, request_data.GetHandle(),
                                    reply.As<v8::Function>())) {
    if (!pending->replied) {
      CompleteWithError(pending->response,
                        "Lynx HTTP request handler is unavailable");
    }
  }
}

class LynxHttpServiceImpl final : public lynx::pub::LynxHttpService {
 public:
  void Request(std::shared_ptr<LynxHttpRequest> request,
               std::shared_ptr<LynxHttpResponse> response) override {
    HttpRequestData request_data{
        .url = request->GetUrl(),
        .method = request->GetMethod(),
        .headers = {request->GetHeaders().begin(), request->GetHeaders().end()},
        .body = request->GetBody(),
    };

    if (GlobalThread::CurrentlyOn(GlobalThread::UI)) {
      RequestOnUIThread(std::move(request_data), std::move(response));
      return;
    }

    GlobalThread::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&RequestOnUIThread, std::move(request_data),
                                  std::move(response)));
  }
};

}  // namespace

void RegisterLynxHttpService() {
  static const bool registered = [] {
    lynx::pub::LynxServiceCenter::GetInstance().RegisterService(
        std::make_shared<LynxHttpServiceImpl>());
    return true;
  }();
  static_cast<void>(registered);
}

}  // namespace lynxtron
