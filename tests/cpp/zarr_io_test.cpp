// Runtime round-trip tests for the tensorstore-backed storage layer (kvstore + zarr).
// Built only under COPICK_ENABLE_ZARR.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "copick/array.h"
#include "io/kvstore.hpp"
#include "io/zarr.hpp"

namespace {

std::string unique_root(const std::string& name) {
  std::string tmp = ::testing::TempDir();  // may have a trailing '/'
  if (!tmp.empty() && tmp.back() == '/') tmp.pop_back();
  return "file://" + tmp + "/copick_zarr/" + name;
}

}  // namespace

TEST(KvStore, ByteRoundTripListDelete) {
  auto kv = copick::io::KvStore::open(unique_root("kv"));
  kv.remove_prefix("");  // start clean

  EXPECT_FALSE(kv.exists("a/b.json"));
  kv.write("a/b.json", "{\"x\":1}");
  kv.write("a/c.json", "2");
  kv.write("d/e.json", "3");

  EXPECT_TRUE(kv.exists("a/b.json"));
  auto v = kv.read("a/b.json");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "{\"x\":1}");
  EXPECT_FALSE(kv.read("missing").has_value());

  std::vector<bool> is_dir;
  auto children = kv.list_children("", &is_dir);
  ASSERT_EQ(children.size(), 2u);
  EXPECT_EQ(children[0], "a");
  EXPECT_EQ(children[1], "d");
  EXPECT_TRUE(is_dir[0]);
  EXPECT_TRUE(is_dir[1]);

  auto under_a = kv.list_children("a");
  ASSERT_EQ(under_a.size(), 2u);  // b.json, c.json

  kv.remove("a/b.json");
  EXPECT_FALSE(kv.exists("a/b.json"));
  kv.remove_prefix("a");
  EXPECT_FALSE(kv.exists("a/c.json"));
}

TEST(Zarr, Float32RoundTripAndRegion) {
  auto kv = copick::io::KvStore::open(unique_root("z_f32"));
  const std::string spec = kv.kvstore_spec("tomo.zarr/0");

  copick::Array3D a(copick::DType::Float32, 2, 3, 4);
  float* p = a.data_as<float>();
  for (std::size_t i = 0; i < a.size(); ++i) p[i] = static_cast<float>(i);

  copick::io::zarr_write(spec, a);

  auto info = copick::io::zarr_info(spec);
  EXPECT_EQ(info.nz, 2u);
  EXPECT_EQ(info.ny, 3u);
  EXPECT_EQ(info.nx, 4u);
  EXPECT_EQ(info.dtype, copick::DType::Float32);

  auto b = copick::io::zarr_read(spec);
  ASSERT_EQ(b.size(), a.size());
  const float* q = b.data_as<float>();
  for (std::size_t i = 0; i < a.size(); ++i) EXPECT_FLOAT_EQ(q[i], static_cast<float>(i));

  // region z[1,2) y[0,2) x[1,4) -> shape (1,2,3); (0,0,0) maps to original (1,0,1)=13
  copick::Region r;
  r.z.start = 1;
  r.z.stop = 2;
  r.y.start = 0;
  r.y.stop = 2;
  r.x.start = 1;
  r.x.stop = 4;
  auto sub = copick::io::zarr_read(spec, r);
  EXPECT_EQ(sub.shape_z(), 1u);
  EXPECT_EQ(sub.shape_y(), 2u);
  EXPECT_EQ(sub.shape_x(), 3u);
  EXPECT_FLOAT_EQ(sub.data_as<float>()[0], 13.0f);
}

TEST(Zarr, UInt16CompressedRoundTrip) {
  auto kv = copick::io::KvStore::open(unique_root("z_u16"));
  const std::string spec = kv.kvstore_spec("seg.zarr/0");

  copick::Array3D a(copick::DType::UInt16, 3, 4, 5);
  std::uint16_t* p = a.data_as<std::uint16_t>();
  for (std::size_t i = 0; i < a.size(); ++i) p[i] = static_cast<std::uint16_t>(i * 7 % 65535);

  copick::io::ZarrWriteOptions opts;  // compress=true (zstd), whole-array chunk
  copick::io::zarr_write(spec, a, opts);

  auto b = copick::io::zarr_read(spec);
  ASSERT_EQ(b.size(), a.size());
  EXPECT_EQ(b.dtype(), copick::DType::UInt16);
  const std::uint16_t* q = b.data_as<std::uint16_t>();
  for (std::size_t i = 0; i < a.size(); ++i)
    EXPECT_EQ(q[i], static_cast<std::uint16_t>(i * 7 % 65535));
}

TEST(Zarr, WriteRegionIntoExisting) {
  auto kv = copick::io::KvStore::open(unique_root("z_region"));
  const std::string spec = kv.kvstore_spec("vol.zarr/0");

  copick::Array3D base(copick::DType::Int32, 4, 4, 4);  // zeros
  copick::io::zarr_write(spec, base);

  // write a 2x2x2 block of 9s at offset (1,1,1)
  copick::Array3D block(copick::DType::Int32, 2, 2, 2);
  std::int32_t* bp = block.data_as<std::int32_t>();
  for (std::size_t i = 0; i < block.size(); ++i) bp[i] = 9;
  copick::Region r;
  r.z.start = 1;
  r.z.stop = 3;
  r.y.start = 1;
  r.y.stop = 3;
  r.x.start = 1;
  r.x.stop = 3;
  copick::io::zarr_write_region(spec, block, r);

  auto full = copick::io::zarr_read(spec);
  const std::int32_t* fp = full.data_as<std::int32_t>();
  auto idx = [](std::size_t z, std::size_t y, std::size_t x) { return (z * 4 + y) * 4 + x; };
  EXPECT_EQ(fp[idx(1, 1, 1)], 9);
  EXPECT_EQ(fp[idx(2, 2, 2)], 9);
  EXPECT_EQ(fp[idx(0, 0, 0)], 0);  // untouched
  EXPECT_EQ(fp[idx(3, 3, 3)], 0);  // untouched
}
