// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "uniplugin/uniplugin_globals.h"

#include "base/no_destructor.h"

namespace uniplugin {

UnipluginGlobal& UnipluginGlobal::GetInstance() {
  static base::NoDestructor<UnipluginGlobal> instance;
  return *instance;
}

scoped_refptr<base::SingleThreadTaskRunner> uniplugin_task_runner() {
  return UnipluginGlobal::GetInstance().uniplugin_thread_->task_runner();
}

}  // namespace uniplugin
