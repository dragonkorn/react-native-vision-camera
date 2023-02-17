//
// Created by Marc on 19/06/2021.
//

#include "FrameHostObject.h"
#include <android/log.h>
#include <fbjni/fbjni.h>
#include <jni.h>
#include <vector>
#include <string>
#include <JsiHostObject.h>

namespace vision {

using namespace facebook;

FrameHostObject::FrameHostObject(jni::alias_ref<JImageProxy::javaobject> image): frame(make_global(image)), _refCount(0) { }

FrameHostObject::~FrameHostObject() {
  // Hermes' Garbage Collector (Hades GC) calls destructors on a separate Thread
  // which might not be attached to JNI. Ensure that we use the JNI class loader when
  // deallocating the `frame` HybridClass, because otherwise JNI cannot call the Java
  // destroy() function.
  jni::ThreadScope::WithClassLoader([&] { frame.reset(); });
}

std::vector<jsi::PropNameID> FrameHostObject::getPropertyNames(jsi::Runtime& rt) {
  std::vector<jsi::PropNameID> result;
  result.push_back(jsi::PropNameID::forUtf8(rt, std::string("width")));
  result.push_back(jsi::PropNameID::forUtf8(rt, std::string("height")));
  result.push_back(jsi::PropNameID::forUtf8(rt, std::string("bytesPerRow")));
  result.push_back(jsi::PropNameID::forUtf8(rt, std::string("planesCount")));
  // Debugging
  result.push_back(jsi::PropNameID::forUtf8(rt, std::string("toString")));
  // Ref Management
  result.push_back(jsi::PropNameID::forUtf8(rt, std::string("isValid")));
  result.push_back(jsi::PropNameID::forUtf8(rt, std::string("incrementRefCount")));
  result.push_back(jsi::PropNameID::forUtf8(rt, std::string("decrementRefCount")));
  return result;
}

jsi::Value FrameHostObject::get(jsi::Runtime& runtime, const jsi::PropNameID& propName) {
  auto name = propName.utf8(runtime);

  if (name == "toString") {
    auto toString = JSI_HOST_FUNCTION_LAMBDA {
      if (!this->frame) {
        return jsi::String::createFromUtf8(runtime, "[closed frame]");
      }
      auto width = this->frame->getWidth();
      auto height = this->frame->getHeight();
      auto str = std::to_string(width) + " x " + std::to_string(height) + " Frame";
      return jsi::String::createFromUtf8(runtime, str);
    };
    return jsi::Function::createFromHostFunction(runtime, jsi::PropNameID::forUtf8(runtime, "toString"), 0, toString);
  }
  if (name == "incrementRefCount") {
    auto incrementRefCount = JSI_HOST_FUNCTION_LAMBDA {
      // Increment retain count by one so ARC doesn't destroy the Frame Buffer.
      std::lock_guard lock(this->_refCountMutex);
      this->_refCount++;
      return jsi::Value::undefined();
    };
    return jsi::Function::createFromHostFunction(runtime,
                                                 jsi::PropNameID::forUtf8(runtime, "incrementRefCount"),
                                                 0,
                                                 incrementRefCount);
  }

  if (name == "decrementRefCount") {
    auto decrementRefCount = JSI_HOST_FUNCTION_LAMBDA {
      // Decrement retain count by one. If the retain count is zero, ARC will destroy the Frame Buffer.
      std::lock_guard lock(this->_refCountMutex);
      this->_refCount--;
      if (_refCount < 1) {
        this->frame->close();
      }
      return jsi::Value::undefined();
    };
    return jsi::Function::createFromHostFunction(runtime,
                                                 jsi::PropNameID::forUtf8(runtime, "decrementRefCount"),
                                                 0,
                                                 decrementRefCount);
  }

  if (name == "isValid") {
    return jsi::Value(this->frame && this->frame->getIsValid());
  }
  if (name == "width") {
    return jsi::Value(this->frame->getWidth());
  }
  if (name == "height") {
    return jsi::Value(this->frame->getHeight());
  }
  if (name == "bytesPerRow") {
    return jsi::Value(this->frame->getBytesPerRow());
  }
  if (name == "planesCount") {
    return jsi::Value(this->frame->getPlanesCount());
  }

  // fallback to base implementation
  return HostObject::get(runtime, propName);
}

} // namespace vision