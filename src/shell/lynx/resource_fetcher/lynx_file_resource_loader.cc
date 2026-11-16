// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/lynx/resource_fetcher/lynx_file_resource_loader.h"

#include "shell/common/asar/asar_util.h"
#include "shell/common/file_url_util.h"
#include "shell/common/thread_restrictions.h"
#include "url/gurl.h"

namespace lynxtron::resource_fetcher {

bool TryLoadFileResource(
    const std::string& url,
    const std::shared_ptr<lynx::pub::resource::LynxResourceResponse>&
        response) {
  GURL file_url(url);
  if (!file_url.SchemeIsFile()) {
    return false;
  }

  base::FilePath file_path;
  std::string contents;
  ScopedAllowBlockingForLynxtron allow_blocking;
  if (!FileURLToFilePath(file_url, &file_path) ||
      !asar::ReadFileToString(file_path, &contents)) {
    response->SetCode(1);
    response->SetErrorMessage("Failed to read file");
  } else {
    response->SetCode(0);
    response->SetData(reinterpret_cast<uint8_t*>(contents.data()),
                      contents.size());
  }
  response->Complete();
  return true;
}

}  // namespace lynxtron::resource_fetcher
