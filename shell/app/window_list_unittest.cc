// Copyright 2025 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "lynxtron/shell/app/window_list.h"

#include "base/memory/weak_ptr.h"
#include "lynxtron/shell/app/native_window.h"
#include "lynxtron/shell/app/window_list_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lynxtron {

class MockNativeWindow : public NativeWindow {
 public:
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, CloseImmediately, (), (override));
  MOCK_METHOD(bool, IsClosed, (), (const, override));

  base::WeakPtr<NativeWindow> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<NativeWindow> weak_factory_{this};
};

class MockWindowListObserver : public WindowListObserver {
 public:
  MOCK_METHOD(void, OnWindowAllClosed, (), (override));
};

class WindowListTest : public testing::Test {
 public:
  WindowListTest() = default;

 protected:
  void TearDown() override {
    // Ensure the window list is empty between tests.
    WindowList::DestroyAllWindows();
  }
};

TEST_F(WindowListTest, AddAndRemoveWindow) {
  MockNativeWindow window1;
  MockNativeWindow window2;

  EXPECT_TRUE(WindowList::IsEmpty());

  WindowList::AddWindow(&window1);
  EXPECT_FALSE(WindowList::IsEmpty());
  EXPECT_EQ(WindowList::GetWindows().size(), 1u);

  WindowList::AddWindow(&window2);
  EXPECT_EQ(WindowList::GetWindows().size(), 2u);

  WindowList::RemoveWindow(&window1);
  EXPECT_EQ(WindowList::GetWindows().size(), 1u);

  MockWindowListObserver observer;
  WindowList::AddObserver(&observer);

  // Expect OnWindowAllClosed to be called when the last window is removed.
  EXPECT_CALL(observer, OnWindowAllClosed()).Times(1);
  WindowList::RemoveWindow(&window2);
  EXPECT_TRUE(WindowList::IsEmpty());

  WindowList::RemoveObserver(&observer);
}

TEST_F(WindowListTest, CloseAllWindows) {
  MockNativeWindow window1;
  MockNativeWindow window2;

  ON_CALL(window1, IsClosed()).WillByDefault(testing::Return(false));
  ON_CALL(window2, IsClosed()).WillByDefault(testing::Return(false));

  WindowList::AddWindow(&window1);
  WindowList::AddWindow(&window2);

  EXPECT_CALL(window1, Close()).Times(1);
  EXPECT_CALL(window2, Close()).Times(1);

  WindowList::CloseAllWindows();
}

TEST_F(WindowListTest, DestroyAllWindows) {
  MockNativeWindow window1;
  MockNativeWindow window2;

  ON_CALL(window1, IsClosed()).WillByDefault(testing::Return(false));
  ON_CALL(window2, IsClosed()).WillByDefault(testing::Return(false));

  WindowList::AddWindow(&window1);
  WindowList::AddWindow(&window2);

  EXPECT_CALL(window1, CloseImmediately()).Times(1);
  EXPECT_CALL(window2, CloseImmediately()).Times(1);

  WindowList::DestroyAllWindows();
}

}  // namespace lynxtron
