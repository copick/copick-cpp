#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

#include "copick/array.h"

using copick::Array3D;
using copick::DType;

TEST(Array3D, AllocatesZeroed) {
  Array3D a(DType::Float32, 2, 3, 4);
  EXPECT_EQ(a.shape_z(), 2u);
  EXPECT_EQ(a.shape_y(), 3u);
  EXPECT_EQ(a.shape_x(), 4u);
  EXPECT_EQ(a.size(), 24u);
  EXPECT_EQ(a.nbytes(), 24u * 4u);
  const float* p = a.data_as<float>();
  for (std::size_t i = 0; i < a.size(); ++i) EXPECT_FLOAT_EQ(p[i], 0.0f);
}

TEST(Array3D, TypedWriteRead) {
  Array3D a(DType::Int16, 1, 1, 3);
  std::int16_t* p = a.data_as<std::int16_t>();
  p[0] = 1;
  p[1] = -2;
  p[2] = 300;
  EXPECT_EQ(a.data_as<std::int16_t>()[2], 300);
}

TEST(Array3D, DtypeMismatchThrows) {
  Array3D a(DType::UInt8, 1, 1, 1);
  EXPECT_THROW(a.data_as<float>(), std::runtime_error);
}

TEST(Array3D, DtypeSizes) {
  EXPECT_EQ(copick::dtype_size(DType::Float64), 8u);
  EXPECT_EQ(copick::dtype_size(DType::UInt8), 1u);
  EXPECT_EQ(copick::dtype_size(DType::Int32), 4u);
}

TEST(Range, DefaultsToFullAxis) {
  copick::Range r;
  EXPECT_EQ(r.start, 0u);
  EXPECT_EQ(r.stop, copick::Range::kEnd);
}
