// Array3D — an owning, contiguous 3D array that copick's to_array()/from_array()/set_region()
// pass around. Axis order is (z, y, x) to match copick/zarr on-disk layout.
//
// PUBLIC HEADER — must compile under -std=c++11. Deliberately dependency-free so it can
// cross the ABI boundary and be consumed from AreTomo3's C++11 build.
#ifndef COPICK_ARRAY_H
#define COPICK_ARRAY_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "copick/export.h"

namespace copick {

/// Element data type of an Array3D. Matches the dtypes copick tomograms/segmentations use.
enum class DType {
  Float32,
  Float64,
  Int8,
  UInt8,
  Int16,
  UInt16,
  Int32,
  UInt32,
  Int64,
  UInt64,
};

/// Number of bytes in one element of the given dtype.
COPICK_API std::size_t dtype_size(DType dtype);

/// A dense 3D array with (z, y, x) axis order and contiguous, row-major storage
/// (x fastest). Holds raw bytes plus a dtype tag; typed views are obtained via data_as<T>().
class COPICK_API Array3D {
 public:
  Array3D() {}

  /// Allocate a (z, y, x) array of the given dtype, zero-initialized.
  Array3D(DType dtype, std::size_t nz, std::size_t ny, std::size_t nx)
      : dtype_(dtype), nz_(nz), ny_(ny), nx_(nx), data_(nz * ny * nx * dtype_size(dtype), 0) {}

  DType dtype() const { return dtype_; }
  std::size_t shape_z() const { return nz_; }
  std::size_t shape_y() const { return ny_; }
  std::size_t shape_x() const { return nx_; }
  std::size_t size() const { return nz_ * ny_ * nx_; }  ///< number of elements
  std::size_t nbytes() const { return data_.size(); }

  /// Raw byte access.
  std::uint8_t* bytes() { return data_.empty() ? nullptr : &data_[0]; }
  const std::uint8_t* bytes() const { return data_.empty() ? nullptr : &data_[0]; }

  /// Typed pointer to the first element. Throws std::runtime_error if sizeof(T) does not
  /// match the element size (a cheap guard against a dtype mismatch).
  template <class T>
  T* data_as() {
    if (sizeof(T) != dtype_size(dtype_)) throw std::runtime_error("Array3D: dtype size mismatch");
    return reinterpret_cast<T*>(bytes());
  }
  template <class T>
  const T* data_as() const {
    if (sizeof(T) != dtype_size(dtype_)) throw std::runtime_error("Array3D: dtype size mismatch");
    return reinterpret_cast<const T*>(bytes());
  }

 private:
  DType dtype_ = DType::Float32;
  std::size_t nz_ = 0;
  std::size_t ny_ = 0;
  std::size_t nx_ = 0;
  std::vector<std::uint8_t> data_;
};

/// A half-open axis range [start, stop). stop == kEnd means "to the end of the axis".
struct Range {
  static const std::size_t kEnd = static_cast<std::size_t>(-1);
  std::size_t start = 0;
  std::size_t stop = kEnd;
};

/// A (z, y, x) region-of-interest for windowed reads/writes (copick's slice arguments).
struct Region {
  Range z;
  Range y;
  Range x;
};

}  // namespace copick

#endif  // COPICK_ARRAY_H
