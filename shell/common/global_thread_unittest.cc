// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "shell/common/global_thread.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lynxtron {

class GlobalThreadTest : public testing::Test {
 protected:
  void SetUp() override { global_thread_ = std::make_unique<GlobalThread>(); }

  void TearDown() override { global_thread_.reset(); }

  std::unique_ptr<GlobalThread> global_thread_;
};

TEST_F(GlobalThreadTest, BasicInitialization) {
  EXPECT_TRUE(GlobalThread::IsThreadInitialized(GlobalThread::UI));
  EXPECT_TRUE(GlobalThread::IsThreadInitialized(GlobalThread::IO));

  EXPECT_STREQ(GlobalThread::GetThreadName(GlobalThread::UI), "UI");
  EXPECT_STREQ(GlobalThread::GetThreadName(GlobalThread::IO), "IO");
}

TEST_F(GlobalThreadTest, CurrentlyOn) {
  // The test runs on the UI thread since GlobalThread initializes
  // the main thread as the UI thread.
  EXPECT_TRUE(GlobalThread::CurrentlyOn(GlobalThread::UI));
  EXPECT_FALSE(GlobalThread::CurrentlyOn(GlobalThread::IO));
}

TEST_F(GlobalThreadTest, GetTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_runner =
      GlobalThread::GetUIThreadTaskRunner();
  scoped_refptr<base::SingleThreadTaskRunner> io_runner =
      GlobalThread::GetIOThreadTaskRunner();

  EXPECT_TRUE(ui_runner);
  EXPECT_TRUE(io_runner);

  EXPECT_EQ(ui_runner, GlobalThread::GetTaskRunnerForThread(GlobalThread::UI));
  EXPECT_EQ(io_runner, GlobalThread::GetTaskRunnerForThread(GlobalThread::IO));
}

TEST_F(GlobalThreadTest, PostTaskToIOThread) {
  base::RunLoop run_loop;
  bool ran_on_io = false;

  GlobalThread::GetIOThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](bool* ran, base::OnceClosure quit_closure) {
                       *ran = GlobalThread::CurrentlyOn(GlobalThread::IO);
                       std::move(quit_closure).Run();
                     },
                     &ran_on_io, run_loop.QuitClosure()));

  run_loop.Run();
  EXPECT_TRUE(ran_on_io);
}

namespace {
struct DeleteObserver {
  DeleteObserver(bool* deleted, bool* deleted_on_io)
      : deleted(deleted), deleted_on_io(deleted_on_io) {}
  ~DeleteObserver() {
    *deleted = true;
    *deleted_on_io = GlobalThread::CurrentlyOn(GlobalThread::IO);
  }
  bool* deleted;
  bool* deleted_on_io;
};
}  // namespace

TEST_F(GlobalThreadTest, DeleteOnIOThread) {
  bool deleted = false;
  bool deleted_on_io = false;

  {
    std::unique_ptr<DeleteObserver, GlobalThread::DeleteOnIOThread> ptr(
        new DeleteObserver(&deleted, &deleted_on_io));
    // ptr goes out of scope and will post a delete task to the IO thread.
  }

  // It should not be deleted immediately on the UI thread.
  EXPECT_FALSE(deleted);

  // Run pending tasks on IO thread and then back to UI thread to verify.
  base::RunLoop run_loop;
  GlobalThread::GetIOThreadTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce([]() {}), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(deleted);
  EXPECT_TRUE(deleted_on_io);
}

}  // namespace lynxtron
