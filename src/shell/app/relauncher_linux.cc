// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#include "shell/app/relauncher.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
namespace relauncher::internal {
void RelauncherSynchronizeWithParent() {
  base::ScopedFD sync(kRelauncherSyncFD);
  pid_t parent = getppid();
  base::ScopedFD pidfd(syscall(SYS_pidfd_open, parent, 0));
  if (HANDLE_EINTR(write(sync.get(), "", 1)) != 1) {
    return;
  }
  if (pidfd.is_valid()) {
    struct pollfd descriptor{pidfd.get(), POLLIN, 0};
    HANDLE_EINTR(poll(&descriptor, 1, -1));
  } else {
    while (getppid() == parent) {
      usleep(10000);
    }
  }
}
int LaunchProgram(const StringVector&, const StringVector& argv) {
  base::ScopedFD devnull(HANDLE_EINTR(open("/dev/null", O_WRONLY)));
  if (!devnull.is_valid() || argv.empty()) {
    return 1;
  }
  base::LaunchOptions options;
  options.new_process_group = true;
  options.fds_to_remap.emplace_back(devnull.get(), STDERR_FILENO);
  options.fds_to_remap.emplace_back(devnull.get(), STDOUT_FILENO);
  return base::LaunchProcess(argv, options).IsValid() ? 0 : 1;
}
}  // namespace relauncher::internal
