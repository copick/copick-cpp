#include "copick/array.h"

namespace copick {

// Out-of-line definition in case kEnd is ODR-used (e.g. bound to a reference).
const std::size_t Range::kEnd;

std::size_t dtype_size(DType dtype) {
  switch (dtype) {
    case DType::Int8:
    case DType::UInt8:
      return 1;
    case DType::Int16:
    case DType::UInt16:
      return 2;
    case DType::Float32:
    case DType::Int32:
    case DType::UInt32:
      return 4;
    case DType::Float64:
    case DType::Int64:
    case DType::UInt64:
      return 8;
  }
  return 0;
}

}  // namespace copick
