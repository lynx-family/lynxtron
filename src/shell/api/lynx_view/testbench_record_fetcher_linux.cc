// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/api/lynx_view/testbench_record_fetcher.h"

#include <curl/curl.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/task/thread_pool.h"

namespace lynxtron {
namespace {

size_t AppendResponseBody(char* data,
                          size_t size,
                          size_t count,
                          void* user_data) {
  auto* response = static_cast<std::string*>(user_data);
  const size_t bytes = size * count;
  response->append(data, bytes);
  return bytes;
}

std::string FetchRecordBlocking(const std::string& url) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    return "";
  }

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendResponseBody);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  const CURLcode result = curl_easy_perform(curl);
  long status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK ||
      (status_code != 0 && (status_code < 200 || status_code >= 300))) {
    LOG(ERROR) << "Failed to fetch TestBench record " << url
               << ": curl=" << curl_easy_strerror(result)
               << ", HTTP=" << status_code;
    return "";
  }
  LOG(INFO) << "Fetched TestBench record " << url << " (" << response.size()
            << " bytes)";
  return response;
}

void FetchAndRunCallback(const std::string& url,
                         std::unique_ptr<TestbenchRecordCallback> callback) {
  (*callback)(FetchRecordBlocking(url));
}

}  // namespace

void FetchTestbenchRecord(const std::string& url,
                          TestbenchRecordCallback callback) {
  if (!callback) {
    return;
  }

  // TestBenchActionManager synchronously waits for this callback from the UI
  // thread. Do not post the reply back to that thread, or it would deadlock.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &FetchAndRunCallback, url,
          std::make_unique<TestbenchRecordCallback>(std::move(callback))));
}

}  // namespace lynxtron
