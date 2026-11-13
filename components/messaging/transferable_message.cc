// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "components/messaging/public/cpp/transferable_message.h"

namespace messaging {

TransferableMessage::TransferableMessage() = default;
TransferableMessage::TransferableMessage(TransferableMessage&&) = default;
TransferableMessage& TransferableMessage::operator=(TransferableMessage&&) =
    default;
TransferableMessage::~TransferableMessage() = default;

}  // namespace messaging
