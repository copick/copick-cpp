// Image-entity tests: VoxelSpacing -> Tomogram -> Features against a real overlay
// project. Mirrors parts of copick/tests/test_filesystem.py. Built only under
// COPICK_ENABLE_ZARR.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "copick/array.h"
#include "copick/errors.h"
#include "copick/root.h"
#include "copick/run.h"
#include "copick/tomogram.h"
#include "copick/voxel_spacing.h"
#include "io/kvstore.hpp"
#include "io/ome.hpp"

namespace {

std::string overlay_root(const std::string& name) {
  std::string tmp = ::testing::TempDir();
  if (!tmp.empty() && tmp.back() == '/') tmp.pop_back();
  return "file://" + tmp + "/copick_image/" + name;
}

std::string config_for(const std::string& root_url) {
  return std::string(R"({"config_type":"filesystem","pickable_objects":[)") +
         R"({"name":"proteasome","is_particle":true,"label":1,"color":[255,0,0,255]})" +
         R"(],"overlay_root":")" + root_url + R"("})";
}

copick::Array3D ramp(copick::DType dt, std::size_t z, std::size_t y, std::size_t x) {
  copick::Array3D a(dt, z, y, x);
  float* p = a.data_as<float>();
  for (std::size_t i = 0; i < a.size(); ++i) p[i] = static_cast<float>(i);
  return a;
}

}  // namespace

class ImageTest : public ::testing::Test {
 protected:
  std::string url;
  copick::Run run;

  void SetUp() override {
    url = overlay_root(::testing::UnitTest::GetInstance()->current_test_info()->name());
    copick::io::KvStore::open(url).remove_prefix("");
    auto root = copick::from_string(config_for(url));
    run = root.new_run("TS_001");
  }
};

TEST_F(ImageTest, VoxelSpacingCreateEnumerateGet) {
  run.new_voxel_spacing(20.0);
  auto vs = run.new_voxel_spacing(10.0);
  EXPECT_DOUBLE_EQ(vs.voxel_size(), 10.0);
  EXPECT_EQ(vs.run().name(), "TS_001");

  auto all = run.voxel_spacings();
  ASSERT_EQ(all.size(), 2u);
  EXPECT_DOUBLE_EQ(all[0].voxel_size(), 10.0);  // sorted
  EXPECT_DOUBLE_EQ(all[1].voxel_size(), 20.0);

  EXPECT_TRUE(run.get_voxel_spacing(10.0).valid());
  EXPECT_FALSE(run.get_voxel_spacing(15.0).valid());
  EXPECT_THROW(run.new_voxel_spacing(10.0), copick::ValidationError);
  EXPECT_TRUE(run.new_voxel_spacing(10.0, /*exist_ok=*/true).valid());
}

TEST_F(ImageTest, TomogramWriteReadRegionAndMetadata) {
  auto vs = run.new_voxel_spacing(10.0);
  auto tomo = vs.new_tomogram("wbp");
  EXPECT_DOUBLE_EQ(tomo.voxel_size(), 10.0);

  tomo.from_numpy(ramp(copick::DType::Float32, 2, 3, 4));

  // Appears in enumeration once written.
  auto tomos = vs.tomograms();
  ASSERT_EQ(tomos.size(), 1u);
  EXPECT_EQ(tomos[0].tomo_type(), "wbp");
  EXPECT_TRUE(vs.get_tomogram("wbp").valid());
  EXPECT_FALSE(vs.get_tomogram("sirt").valid());

  auto full = tomo.numpy();
  ASSERT_EQ(full.size(), 24u);
  EXPECT_FLOAT_EQ(full.data_as<float>()[23], 23.0f);

  copick::Region r;
  r.z.start = 1;
  r.z.stop = 2;
  auto sub = tomo.numpy(r);
  EXPECT_EQ(sub.shape_z(), 1u);
  EXPECT_FLOAT_EQ(sub.data_as<float>()[0], 12.0f);  // (1,0,0) = 12

  // The OME group .zattrs encodes the voxel size.
  auto kv = copick::io::KvStore::open(url);
  auto zattrs = kv.read("ExperimentRuns/TS_001/VoxelSpacing10.000/wbp.zarr/.zattrs");
  ASSERT_TRUE(zattrs.has_value());
  EXPECT_DOUBLE_EQ(copick::io::voxel_size_from_zattrs(*zattrs), 10.0);

  // Parent chain.
  EXPECT_EQ(tomo.voxel_spacing().run().name(), "TS_001");
}

TEST_F(ImageTest, FeaturesWriteReadEnumerate) {
  auto vs = run.new_voxel_spacing(10.0);
  auto tomo = vs.new_tomogram("wbp");
  tomo.from_numpy(ramp(copick::DType::Float32, 2, 2, 2));

  auto feat = tomo.new_features("sobel");
  feat.from_numpy(ramp(copick::DType::Float32, 2, 2, 2));

  auto feats = tomo.features();
  ASSERT_EQ(feats.size(), 1u);
  EXPECT_EQ(feats[0].feature_type(), "sobel");
  EXPECT_EQ(feats[0].tomo_type(), "wbp");
  EXPECT_TRUE(tomo.get_features("sobel").valid());
  EXPECT_FALSE(tomo.get_features("nope").valid());

  auto arr = feat.numpy();
  EXPECT_EQ(arr.size(), 8u);
  EXPECT_FLOAT_EQ(arr.data_as<float>()[7], 7.0f);

  // Features do not appear as a tomogram (name contains "features").
  EXPECT_EQ(vs.tomograms().size(), 1u);
}

TEST_F(ImageTest, SetRegionOnTomogram) {
  auto vs = run.new_voxel_spacing(10.0);
  auto tomo = vs.new_tomogram("wbp");
  tomo.from_numpy(copick::Array3D(copick::DType::Float32, 4, 4, 4));  // zeros

  copick::Array3D block(copick::DType::Float32, 2, 2, 2);
  float* bp = block.data_as<float>();
  for (std::size_t i = 0; i < block.size(); ++i) bp[i] = 5.0f;
  copick::Region r;
  r.z.start = 1;
  r.z.stop = 3;
  r.y.start = 1;
  r.y.stop = 3;
  r.x.start = 1;
  r.x.stop = 3;
  tomo.set_region(block, r);

  auto full = tomo.numpy();
  const float* fp = full.data_as<float>();
  auto idx = [](std::size_t z, std::size_t y, std::size_t x) { return (z * 4 + y) * 4 + x; };
  EXPECT_FLOAT_EQ(fp[idx(1, 1, 1)], 5.0f);
  EXPECT_FLOAT_EQ(fp[idx(0, 0, 0)], 0.0f);
}
