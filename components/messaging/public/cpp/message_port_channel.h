// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef COMPONENTS_MESSAGING_PUBLIC_CPP_MESSAGE_PORT_CHANNEL_H_
#define COMPONENTS_MESSAGING_PUBLIC_CPP_MESSAGE_PORT_CHANNEL_H_

#include <vector>

#include "base/memory/scoped_refptr.h"

namespace messaging {

class MessagePortDescriptor;

// MessagePortChannel corresponds to a HTML MessagePort. It is a thin wrapper
// around a MessagePortDescriptor and used to provide methods for reading and
// writing messages. Currently all reading and writing is handled separately
// by other code, so MessagePortChannel is nothing other than a ref-counted
// holder of a MessagePortDescriptor, and is in the process of being removed.
class MessagePortChannel {
 public:
  ~MessagePortChannel();
  MessagePortChannel();

  // Shallow copy, resulting in multiple references to the same port.
  MessagePortChannel(const MessagePortChannel& other);
  MessagePortChannel& operator=(const MessagePortChannel& other);

  explicit MessagePortChannel(MessagePortDescriptor handle);

  const MessagePortDescriptor& GetHandle() const;
  MessagePortDescriptor ReleaseHandle() const;

  static std::vector<MessagePortDescriptor> ReleaseHandles(
      const std::vector<MessagePortChannel>& ports);
  static std::vector<MessagePortChannel> CreateFromHandles(
      std::vector<MessagePortDescriptor> handles);

 private:
  class State;
  mutable scoped_refptr<State> state_;
};

}  // namespace messaging

#endif  // COMPONENTS_MESSAGING_PUBLIC_CPP_MESSAGE_PORT_CHANNEL_H_
