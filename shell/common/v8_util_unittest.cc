#include "lynxtron/shell/common/v8_util.h"

#include "base/test/task_environment.h"
#include "gin/public/isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

// TODO(Guo Xi): review unittest

namespace lynxtron {
class V8UtilTest : public testing::Test {
 public:
  V8UtilTest()
      : isolate_holder_(task_environment_.GetMainThreadTaskRunner(),
                        gin::IsolateHolder::IsolateType::kTest),
        isolate_scope_(isolate_holder_.isolate()),
        isolate_(isolate_holder_.isolate()) {}

  void SetUp() override {
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context = v8::Context::New(isolate_);
    context_.Reset(isolate_, context);
  }

  void TearDown() override { context_.Reset(); }

  v8::Isolate* isolate() { return isolate_; }
  v8::Local<v8::Context> GetContext() {
    return v8::Local<v8::Context>::New(isolate(), context_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  gin::IsolateHolder isolate_holder_;
  v8::Isolate::Scope isolate_scope_;
  raw_ptr<v8::Isolate> isolate_;
  v8::Persistent<v8::Context> context_;
};

TEST_F(V8UtilTest, SerializeAndDeserializeV8Value) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope context_scope(context);

  // 1. Create a V8 value to serialize.
  v8::Local<v8::String> original_value =
      v8::String::NewFromUtf8(isolate(), "hello world",
                              v8::NewStringType::kNormal)
          .ToLocalChecked();

  // 2. Serialize the value.
  std::vector<uint8_t> data;
  ASSERT_TRUE(SerializeV8Value(isolate(), original_value, data));
  ASSERT_FALSE(data.empty());

  // 3. Deserialize the value.
  v8::Local<v8::Value> deserialized_value = DeserializeV8Value(isolate(), data);

  // 4. Verify the result.
  ASSERT_FALSE(deserialized_value.IsEmpty());
  ASSERT_TRUE(deserialized_value->IsString());
  v8::Local<v8::String> deserialized_string =
      deserialized_value.As<v8::String>();

  v8::String::Utf8Value original_utf8(isolate(), original_value);
  v8::String::Utf8Value deserialized_utf8(isolate(), deserialized_string);

  EXPECT_STREQ(*original_utf8, *deserialized_utf8);
}

TEST_F(V8UtilTest, SerializeAndDeserializeJSONObject) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = GetContext();
  v8::Context::Scope context_scope(context);

  // 1. Create a V8 object from a JSON string.
  v8::Local<v8::String> json_string =
      v8::String::NewFromUtf8(isolate(), "{\"key\":\"value\",\"number\":42}",
                              v8::NewStringType::kNormal)
          .ToLocalChecked();
  v8::Local<v8::Value> json_value =
      v8::JSON::Parse(context, json_string).ToLocalChecked();
  ASSERT_TRUE(json_value->IsObject());
  v8::Local<v8::Object> original_object = json_value.As<v8::Object>();

  // 2. Serialize the object.
  std::vector<uint8_t> data;
  ASSERT_TRUE(SerializeV8Value(isolate(), original_object, data));
  ASSERT_FALSE(data.empty());

  // 3. Deserialize the object.
  v8::Local<v8::Value> deserialized_value = DeserializeV8Value(isolate(), data);
  ASSERT_FALSE(deserialized_value.IsEmpty());
  ASSERT_TRUE(deserialized_value->IsObject());

  // 4. Verify the result by stringifying the deserialized object and
  // comparing it to the original JSON string.
  v8::Local<v8::String> stringified_deserialized =
      v8::JSON::Stringify(context, deserialized_value).ToLocalChecked();

  v8::String::Utf8Value original_utf8(isolate(), json_string);
  v8::String::Utf8Value deserialized_utf8(isolate(), stringified_deserialized);

  EXPECT_STREQ(*original_utf8, *deserialized_utf8);
}

}  // namespace lynxtron
