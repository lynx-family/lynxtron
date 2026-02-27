// Copyright (c) 2020 Slack Technologies, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/common/v8_util.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "gin/converter.h"
#include "v8/include/v8-value-serializer.h"
#include "v8/include/v8.h"

namespace lynxtron {

class V8Serializer : public v8::ValueSerializer::Delegate {
 public:
  explicit V8Serializer(v8::Isolate* isolate)
      : isolate_(isolate), serializer_(isolate, this) {}
  ~V8Serializer() override = default;

  bool Serialize(v8::Local<v8::Value> value, std::vector<uint8_t>& out) {
    v8::MicrotasksScope microtasks_scope(
        isolate_->GetCurrentContext(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);

    serializer_.WriteHeader();
    bool wrote_value;
    if (!serializer_.WriteValue(isolate_->GetCurrentContext(), value)
             .To(&wrote_value)) {
      isolate_->ThrowException(v8::Exception::Error(
          gin::StringToV8(isolate_, "An object could not be cloned.")));
      return false;
    }
    DCHECK(wrote_value);

    const auto [data_bytes, data_len] = serializer_.Release();
    DCHECK_EQ(std::data(data_), data_bytes);
    DCHECK_GE(std::size(data_), data_len);
    data_.resize(data_len);
    out = std::move(data_);
    return true;
  }

  // v8::ValueSerializer::Delegate
  void* ReallocateBufferMemory(void* old_buffer,
                               size_t size,
                               size_t* actual_size) override {
    DCHECK_EQ(old_buffer, data_.data());
    data_.resize(size);
    *actual_size = data_.capacity();
    return data_.data();
  }

  void FreeBufferMemory(void* buffer) override {
    DCHECK_EQ(buffer, data_.data());
    data_ = {};
  }

  v8::Maybe<bool> WriteHostObject(v8::Isolate* isolate,
                                  v8::Local<v8::Object> object) override {
    return v8::ValueSerializer::Delegate::WriteHostObject(isolate, object);
  }

  void ThrowDataCloneError(v8::Local<v8::String> message) override {
    isolate_->ThrowException(v8::Exception::Error(message));
  }

 private:
  void WriteTag(const uint8_t tag) { serializer_.WriteRawBytes(&tag, 1U); }

  raw_ptr<v8::Isolate> isolate_;
  std::vector<uint8_t> data_;
  v8::ValueSerializer serializer_;
};

// TODO(Guo Xi): review V8Deserializer
class V8Deserializer : public v8::ValueDeserializer::Delegate {
 public:
  V8Deserializer(v8::Isolate* isolate, base::span<const uint8_t> data)
      : isolate_(isolate),
        deserializer_(isolate, data.data(), data.size(), this) {}

  v8::Local<v8::Value> Deserialize() {
    v8::EscapableHandleScope scope(isolate_);
    auto context = isolate_->GetCurrentContext();

    bool read_header;
    if (!deserializer_.ReadHeader(context).To(&read_header)) {
      return v8::Null(isolate_);
    }
    DCHECK(read_header);
    v8::Local<v8::Value> value;
    if (!deserializer_.ReadValue(context).ToLocal(&value)) {
      return v8::Null(isolate_);
    }
    return scope.Escape(value);
  }

  v8::MaybeLocal<v8::Object> ReadHostObject(v8::Isolate* isolate) override {
    uint8_t tag = 0;
    if (!ReadTag(&tag)) {
      return v8::ValueDeserializer::Delegate::ReadHostObject(isolate);
    }
    return v8::ValueDeserializer::Delegate::ReadHostObject(isolate);
  }

 private:
  bool ReadTag(uint8_t* tag) {
    const void* tag_bytes = nullptr;
    if (!deserializer_.ReadRawBytes(1, &tag_bytes)) {
      return false;
    }
    *tag = *reinterpret_cast<const uint8_t*>(tag_bytes);
    return true;
  }

  raw_ptr<v8::Isolate> isolate_;
  v8::ValueDeserializer deserializer_;
};

bool SerializeV8Value(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      std::vector<uint8_t>& out) {
  return V8Serializer(isolate).Serialize(value, out);
}

v8::Local<v8::Value> DeserializeV8Value(v8::Isolate* isolate,
                                        base::span<const uint8_t> data) {
  return V8Deserializer(isolate, data).Deserialize();
}

namespace util {

/**
 * SAFETY: There is not yet any v8::ArrayBufferView API that passes the
 * UNSAFE_BUFFER_USAGE test, so let's isolate the unsafe API here.
 *
 * Where possible, Electron should use spans returned here instead of
 * |v8::ArrayBufferView::Buffer()->Data()|,
 * |v8::ArrayBufferView::ByteOffset()|,
 * |v8::ArrayBufferView::ByteLength()|.
 */
base::span<uint8_t> as_byte_span(v8::Local<v8::ArrayBufferView> val) {
  uint8_t* data = UNSAFE_BUFFERS(static_cast<uint8_t*>(val->Buffer()->Data()) +
                                 val->ByteOffset());
  const size_t size = val->ByteLength();
  return UNSAFE_BUFFERS(base::span{data, size});
}

}  // namespace util
}  // namespace lynxtron
