// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_LYNX_TRACE_LYNX_TRACE_EVENT_H_
#define LYNXTRON_SHELL_LYNX_TRACE_LYNX_TRACE_EVENT_H_

#include <utility>

#include "base/trace/native/trace_event_utils_perfetto.h"

// Type values for identifying types in the TraceValue union.
#define TRACE_VALUE_TYPE_BOOL (static_cast<unsigned char>(1))
#define TRACE_VALUE_TYPE_UINT (static_cast<unsigned char>(2))
#define TRACE_VALUE_TYPE_INT (static_cast<unsigned char>(3))
#define TRACE_VALUE_TYPE_DOUBLE (static_cast<unsigned char>(4))
#define TRACE_VALUE_TYPE_POINTER (static_cast<unsigned char>(5))
#define TRACE_VALUE_TYPE_STRING (static_cast<unsigned char>(6))
#define TRACE_VALUE_TYPE_COPY_STRING (static_cast<unsigned char>(7))
#define TRACE_VALUE_TYPE_CONVERTABLE (static_cast<unsigned char>(8))
#define TRACE_VALUE_TYPE_PROTO (static_cast<unsigned char>(9))

// Flags for changing the behavior of TRACE_EVENT_API_ADD_TRACE_EVENT.
#define TRACE_EVENT_FLAG_NONE (static_cast<unsigned int>(0))
#define TRACE_EVENT_FLAG_COPY (static_cast<unsigned int>(1 << 0))
#define TRACE_EVENT_FLAG_HAS_ID (static_cast<unsigned int>(1 << 1))
#define TRACE_EVENT_FLAG_SCOPE_OFFSET (static_cast<unsigned int>(1 << 2))
#define TRACE_EVENT_FLAG_SCOPE_EXTRA (static_cast<unsigned int>(1 << 3))
#define TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP (static_cast<unsigned int>(1 << 4))
#define TRACE_EVENT_FLAG_ASYNC_TTS (static_cast<unsigned int>(1 << 5))
#define TRACE_EVENT_FLAG_BIND_TO_ENCLOSING (static_cast<unsigned int>(1 << 6))
#define TRACE_EVENT_FLAG_FLOW_IN (static_cast<unsigned int>(1 << 7))
#define TRACE_EVENT_FLAG_FLOW_OUT (static_cast<unsigned int>(1 << 8))
#define TRACE_EVENT_FLAG_HAS_CONTEXT_ID (static_cast<unsigned int>(1 << 9))
#define TRACE_EVENT_FLAG_HAS_PROCESS_ID (static_cast<unsigned int>(1 << 10))
#define TRACE_EVENT_FLAG_HAS_LOCAL_ID (static_cast<unsigned int>(1 << 11))
#define TRACE_EVENT_FLAG_HAS_GLOBAL_ID (static_cast<unsigned int>(1 << 12))
#define TRACE_EVENT_FLAG_JAVA_STRING_LITERALS \
  (static_cast<unsigned int>(1 << 16))

#define TRACE_EVENT_FLAG_SCOPE_MASK                          \
  (static_cast<unsigned int>(TRACE_EVENT_FLAG_SCOPE_OFFSET | \
                             TRACE_EVENT_FLAG_SCOPE_EXTRA))

// Event phases.
static constexpr char TRACE_EVENT_PHASE_BEGIN = 'B';
static constexpr char TRACE_EVENT_PHASE_END = 'E';
static constexpr char TRACE_EVENT_PHASE_COMPLETE = 'X';
static constexpr char TRACE_EVENT_PHASE_INSTANT = 'I';
static constexpr char TRACE_EVENT_PHASE_ASYNC_BEGIN = 'S';
static constexpr char TRACE_EVENT_PHASE_ASYNC_STEP_INTO = 'T';
static constexpr char TRACE_EVENT_PHASE_ASYNC_STEP_PAST = 'p';
static constexpr char TRACE_EVENT_PHASE_ASYNC_END = 'F';
static constexpr char TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN = 'b';
static constexpr char TRACE_EVENT_PHASE_NESTABLE_ASYNC_END = 'e';
static constexpr char TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT = 'n';
static constexpr char TRACE_EVENT_PHASE_FLOW_BEGIN = 's';
static constexpr char TRACE_EVENT_PHASE_FLOW_STEP = 't';
static constexpr char TRACE_EVENT_PHASE_FLOW_END = 'f';
static constexpr char TRACE_EVENT_PHASE_METADATA = 'M';
static constexpr char TRACE_EVENT_PHASE_COUNTER = 'C';
static constexpr char TRACE_EVENT_PHASE_SAMPLE = 'P';
static constexpr char TRACE_EVENT_PHASE_CREATE_OBJECT = 'N';
static constexpr char TRACE_EVENT_PHASE_SNAPSHOT_OBJECT = 'O';
static constexpr char TRACE_EVENT_PHASE_DELETE_OBJECT = 'D';
static constexpr char TRACE_EVENT_PHASE_MEMORY_DUMP = 'v';
static constexpr char TRACE_EVENT_PHASE_MARK = 'R';
static constexpr char TRACE_EVENT_PHASE_CLOCK_SYNC = 'c';
static constexpr char TRACE_EVENT_PHASE_ENTER_CONTEXT = '(';
static constexpr char TRACE_EVENT_PHASE_LEAVE_CONTEXT = ')';

// Enum reflecting the scope of an INSTANT event. Must fit within
// TRACE_EVENT_FLAG_SCOPE_MASK.
#define TRACE_EVENT_SCOPE_GLOBAL (static_cast<unsigned char>(0 << 2))
#define TRACE_EVENT_SCOPE_PROCESS (static_cast<unsigned char>(1 << 2))
#define TRACE_EVENT_SCOPE_THREAD (static_cast<unsigned char>(2 << 2))

#define TRACE_STR_COPY(str) str

#define NEED_LEGACY_FLAGS(flags)                                            \
  (flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |       \
            TRACE_EVENT_FLAG_HAS_GLOBAL_ID | TRACE_EVENT_FLAG_ASYNC_TTS |   \
            TRACE_EVENT_FLAG_BIND_TO_ENCLOSING | TRACE_EVENT_FLAG_FLOW_IN | \
            TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_HAS_PROCESS_ID))

template <typename T>
[[maybe_unused]] static T&& DecayStringType(T&& t) {
  return std::forward<T>(t);
}
[[maybe_unused]] static inline const char* DecayStringType(const char* t) {
  return t;
}

#define TRACE_EVENT(category, name, ...)                                      \
  struct INTERNAL_TRACE_EVENT_UID(ScopedEvent) {                              \
    struct EventFinalizer {                                                   \
      /* The parameter is an implementation detail. It allows the          */ \
      /* anonymous struct to use aggregate initialization to invoke the    */ \
      /* lambda (which emits the BEGIN event and returns an integer)       */ \
      /* with the proper reference capture for any                         */ \
      /* TrackEventArgumentFunction in |__VA_ARGS__|. This is required so  */ \
      /* that the scoped event is exactly ONE line and can't escape the    */ \
      /* scope if used in a single line if statement.                      */ \
      EventFinalizer(...) {}                                                  \
      ~EventFinalizer() {                                                     \
        TRACE_EVENT_END(category);                                            \
      }                                                                       \
    } finalizer;                                                              \
  } INTERNAL_TRACE_EVENT_UID(scoped_event) {                                  \
    [&]() {                                                                   \
      TRACE_EVENT_BEGIN(category, name, ##__VA_ARGS__);                       \
      return 0;                                                               \
    }()                                                                       \
  }

#define TRACE_EVENT_BEGIN(category, name, ...) \
  lynx::trace::TraceEventBegin(category, DecayStringType(name), ##__VA_ARGS__)
#define TRACE_EVENT_END(category, ...) \
  lynx::trace::TraceEventEnd(category, ##__VA_ARGS__)
#define TRACE_EVENT_INSTANT(category, name, ...) \
  lynx::trace::TraceEventInstant(category, DecayStringType(name), ##__VA_ARGS__)
#define TRACE_EVENT_CATEGORY_ENABLED(category) \
  lynx::trace::TraceEventCategoryEnabled(category)
#define TRACE_COUNTER(category, track, ...)                                \
  lynx::trace::TraceCounter(category, lynx::perfetto::CounterTrack(track), \
                            ##__VA_ARGS__)
#define TRACE_FLOW_ID() lynx::trace::GetFlowId()
#define TRACE_TIME_NS() lynx::trace::GetTraceTimeNs()

#define TRACE_EVENT0(category, name) TRACE_EVENT(category, name)
#define TRACE_EVENT1(category, name, arg1_name, arg1_val) \
  TRACE_EVENT(category, name, arg1_name, arg1_val)
#define TRACE_EVENT2(category, name, arg1_name, arg1_val, arg2_name, arg2_val) \
  TRACE_EVENT(category, name, arg1_name, arg1_val, arg2_name, arg2_val)

#define TRACE_EVENT_BEGIN0(category, name) TRACE_EVENT_BEGIN(category, name)
#define TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val) \
  TRACE_EVENT_BEGIN(category, name, arg1_name, arg1_val)
#define TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val, arg2_name, \
                           arg2_val)                                       \
  TRACE_EVENT_BEGIN(category, name, arg1_name, arg1_val, arg2_name, arg2_val)

#define TRACE_EVENT_END0(category, name) TRACE_EVENT_END(category)
#define TRACE_EVENT_END1(category, name, arg1_name, arg1_val) \
  TRACE_EVENT_END(category, arg1_name, arg1_val)
#define TRACE_EVENT_END2(category, name, arg1_name, arg1_val, arg2_name, \
                         arg2_val)                                       \
  TRACE_EVENT_END(category, arg1_name, arg1_val, arg2_name, arg2_val)

#define TRACE_EVENT_INSTANT0(category_group, name, scope) \
  TRACE_EVENT_INSTANT(category_group, name)
#define TRACE_EVENT_INSTANT1(category_group, name, scope, arg1_name, arg1_val) \
  TRACE_EVENT_INSTANT(category_group, name, arg1_name, arg1_val)
#define TRACE_EVENT_INSTANT2(category_group, name, scope, arg1_name, arg1_val, \
                             arg2_name, arg2_val)                              \
  TRACE_EVENT_INSTANT(category_group, name, arg1_name, arg1_val, arg2_name,    \
                      arg2_val)

#define TRACE_EVENT_WITH_LEGACY(bind_id, flow_flags)     \
  auto* event = ctx.event();                             \
  if (NEED_LEGACY_FLAGS(flow_flags)) {                   \
    auto legacy_event = event->set_legacy_event();       \
    legacy_event->set_bind_id(bind_id);                  \
    if (flow_flags & TRACE_EVENT_FLAG_ASYNC_TTS)         \
      legacy_event->set_use_async_tts(true);             \
    if (flow_flags & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING) \
      legacy_event->set_bind_to_enclosing(true);         \
    const auto kFlowIn = TRACE_EVENT_FLAG_FLOW_IN;       \
    const auto kFlowOut = TRACE_EVENT_FLAG_FLOW_OUT;     \
    const auto kFlowInOut = kFlowIn | kFlowOut;          \
    if ((flow_flags & kFlowInOut) == kFlowInOut) {       \
      legacy_event->set_flow_direction(                  \
          lynx::perfetto::FlowDirection::FLOW_INOUT);    \
    } else if (flow_flags & kFlowIn) {                   \
      legacy_event->set_flow_direction(                  \
          lynx::perfetto::FlowDirection::FLOW_IN);       \
    } else if (flow_flags & kFlowOut) {                  \
      legacy_event->set_flow_direction(                  \
          lynx::perfetto::FlowDirection::FLOW_OUT);      \
    }                                                    \
  } else {                                               \
    event->add_flow_ids(bind_id);                        \
  }

#define TRACE_EVENT_WITH_FLOW0(category_group, name, bind_id, flow_flags)   \
  TRACE_EVENT(category_group, name, [&](lynx::perfetto::EventContext ctx) { \
    TRACE_EVENT_WITH_LEGACY(bind_id, flow_flags);                           \
  })

#define TRACE_EVENT_WITH_FLOW1(category_group, name, bind_id, flow_flags,   \
                               arg1_name, arg1_val)                         \
  TRACE_EVENT(category_group, name, [&](lynx::perfetto::EventContext ctx) { \
    TRACE_EVENT_WITH_LEGACY(bind_id, flow_flags);                           \
    lynx::trace::WriteTraceEventArgs(ctx, arg1_name, arg1_val);             \
  })

#define TRACE_EVENT_SAMPLE_WITH_ID1(category_group, name, id, arg1_name,    \
                                    arg1_val)                               \
  TRACE_EVENT(category_group, name, [&](lynx::perfetto::EventContext ctx) { \
    lynx::trace::WriteTraceEventArgs(ctx, "id", id, arg1_name, arg1_val);   \
  })
#define TRACE_EVENT_CATEGORY_GROUP_ENABLED(category, ret)    \
  do {                                                       \
    *ret = lynx::trace::TraceEventCategoryEnabled(category); \
  } while (0)

#define TRACE_DISABLED_BY_DEFAULT(name) "disabled-by-default-" name

#endif  // LYNXTRON_SHELL_LYNX_TRACE_LYNX_TRACE_EVENT_H_
