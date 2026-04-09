// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef LYNXTRON_SHELL_COMMON_NODE_INCLUDES_H_
#define LYNXTRON_SHELL_COMMON_NODE_INCLUDES_H_

// Include common headers for using node APIs.

#ifdef NODE_SHARED_MODE
#define BUILDING_NODE_EXTENSION
#endif

#undef debug_string    // This is defined in macOS SDK in AssertMacros.h.
#undef require_string  // This is defined in macOS SDK in AssertMacros.h.

// TODO(Guo Xi) : what is this define for?
// clang-format off
#include "lynxtron/src/push_and_undef_node_defines.h"
#include "env-inl.h"
#include "env.h"
#include "node.h"
#include "node_buffer.h"
#include "node_builtins.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_object_wrap.h"
#include "node_options-inl.h"
#include "node_options.h"
#include "node_platform.h"
#include "node_report.h"
#include "tracing/agent.h"
#include "lynxtron/src/pop_node_defines.h"
// clang-format on

// Alternative to NODE_BINDING_CONTEXT_AWARE_X.
// Allows to explicitly register builtin bindings instead of using
// __attribute__((constructor)).
#define NODE_LINKED_BINDING_CONTEXT_AWARE(modname, regfunc) \
  NODE_BINDING_CONTEXT_AWARE_CPP(modname, regfunc, nullptr, NM_F_LINKED)

#endif  // LYNXTRON_SHELL_COMMON_NODE_INCLUDES_H_
