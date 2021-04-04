#include "WebAssemblyInstance.h"

#include <cstring>

namespace runtime {
WebAssemblyGlobal::WebAssemblyGlobal(bytecode::ValueType ValueType_)
    : Storage(), ValueType(ValueType_) {
  static_assert(offsetof(WebAssemblyGlobal, Storage) == 0);
  std::memset(&Storage, 0, sizeof(decltype(Storage)));
}

bytecode::ValueType const &WebAssemblyGlobal::getValueType() const {
  return ValueType;
}

#define ASSERT_AND_RET(Type)                                                   \
  do {                                                                         \
    assert(ValueType == bytecode::valuetypes::Type);                           \
    return Storage.Type;                                                       \
  } while (false)

std::int32_t &WebAssemblyGlobal::asI32() { ASSERT_AND_RET(I32); }
std::int64_t &WebAssemblyGlobal::asI64() { ASSERT_AND_RET(I64); }
float &WebAssemblyGlobal::asF32() { ASSERT_AND_RET(F32); }
double &WebAssemblyGlobal::asF64() { ASSERT_AND_RET(F64); }
std::int32_t const &WebAssemblyGlobal::asI32() const { ASSERT_AND_RET(I32); }
std::int64_t const &WebAssemblyGlobal::asI64() const { ASSERT_AND_RET(I64); }
float const &WebAssemblyGlobal::asF32() const { ASSERT_AND_RET(F32); }
double const &WebAssemblyGlobal::asF64() const { ASSERT_AND_RET(F64); }

__sable_global_t *WebAssemblyGlobal::asInstancePtr() {
  return reinterpret_cast<__sable_global_t *>(this);
}

WebAssemblyGlobal *
WebAssemblyGlobal::fromInstancePtr(__sable_global_t *InstancePtr) {
  return reinterpret_cast<WebAssemblyGlobal *>(InstancePtr);
}
} // namespace runtime