// Reads the copick sample project (produced by Python copick, fetched via
// tests/scripts/fetch_sample_data.py) and verifies copick-cpp sees the same structure and
// values. Set COPICK_SAMPLE_DIR to the tree containing ExperimentRuns/ (i.e.
// <fetch_dir>/sample_project/sample_project); the tests skip if it is unset.
//
// Built only under COPICK_ENABLE_ZARR (mesh assertions need COPICK_ENABLE_MESH too).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "copick/copick.h"

namespace {

std::string sample_dir() {
  const char* env = std::getenv("COPICK_SAMPLE_DIR");
  return env ? std::string(env) : std::string();
}

copick::Root open_sample() {
  const std::string cfg =
      std::string(R"({"config_type":"filesystem","pickable_objects":[)") +
      R"({"name":"proteasome","is_particle":true,"label":1,"color":[255,0,0,255]},)" +
      R"({"name":"ribosome","is_particle":true,"label":2,"color":[0,255,0,255]},)" +
      R"({"name":"membrane","is_particle":false,"label":3,"color":[0,0,0,255]}],)" +
      R"("overlay_root":"local://)" + sample_dir() + R"("})";
  return copick::from_string(cfg);
}

}  // namespace

class SampleData : public ::testing::Test {
 protected:
  void SetUp() override {
    if (sample_dir().empty())
      GTEST_SKIP()
          << "COPICK_SAMPLE_DIR not set (point it at the sample .../ExperimentRuns parent)";
  }
};

TEST_F(SampleData, RunsEnumeratedAndHiddenFiltered) {
  auto root = open_sample();
  std::vector<std::string> names;
  for (const auto& r : root.runs()) names.push_back(r.name());
  std::sort(names.begin(), names.end());
  // .hidden_file under ExperimentRuns/ must be filtered out.
  EXPECT_EQ(names, (std::vector<std::string>{"TS_001", "TS_002", "TS_003"}));
}

TEST_F(SampleData, PickableObjects) {
  auto root = open_sample();
  EXPECT_EQ(root.pickable_objects().size(), 3u);
  EXPECT_TRUE(root.get_object("proteasome").is_particle());
  EXPECT_TRUE(root.get_object("ribosome").is_particle());
  EXPECT_FALSE(root.get_object("membrane").is_particle());
}

TEST_F(SampleData, Ts001EntityCounts) {
  auto run = open_sample().get_run("TS_001");
  ASSERT_TRUE(run.valid());

  std::vector<double> vss;
  for (const auto& v : run.voxel_spacings()) vss.push_back(v.voxel_size());
  EXPECT_EQ(vss, (std::vector<double>{10.0, 20.0}));

  EXPECT_EQ(run.picks().size(), 5u);
  EXPECT_EQ(run.meshes().size(), 3u);
  EXPECT_EQ(run.segmentations().size(), 3u);
}

TEST_F(SampleData, TomogramsAndFeatures) {
  auto vs = open_sample().get_run("TS_001").get_voxel_spacing(10.0);

  std::vector<std::string> types;
  for (const auto& t : vs.tomograms()) types.push_back(t.tomo_type());
  std::sort(types.begin(), types.end());
  // "unrelated_directory" (not *.zarr) and the *_features.zarr entries are excluded.
  EXPECT_EQ(types, (std::vector<std::string>{"denoised", "wbp"}));

  auto wbp = vs.get_tomogram("wbp");
  ASSERT_TRUE(wbp.valid());
  copick::Array3D a = wbp.to_array();
  EXPECT_EQ(a.shape_z(), 64u);
  EXPECT_EQ(a.shape_y(), 64u);
  EXPECT_EQ(a.shape_x(), 64u);
  const float* p = a.data_as<float>();
  EXPECT_NEAR(p[0], 0.074735f, 1e-4);                         // wbp[0,0,0]
  EXPECT_NEAR(p[(10 * 64 + 20) * 64 + 30], 0.047407f, 1e-4);  // wbp[10,20,30]

  std::vector<std::string> fts;
  for (const auto& f : wbp.features()) fts.push_back(f.feature_type());
  std::sort(fts.begin(), fts.end());
  EXPECT_EQ(fts, (std::vector<std::string>{"edge", "sobel"}));

  copick::Array3D sob = wbp.get_features("sobel").to_array();
  EXPECT_EQ(sob.shape_z(), 64u);
  EXPECT_EQ(sob.dtype(), copick::DType::Float32);
}

TEST_F(SampleData, RegionRead) {
  auto wbp = open_sample().get_run("TS_001").get_voxel_spacing(10.0).get_tomogram("wbp");
  copick::Region r;
  r.z.stop = 8;
  r.y.stop = 8;
  r.x.stop = 8;
  copick::Array3D sub = wbp.to_array(r);
  EXPECT_EQ(sub.shape_z(), 8u);
  const float* p = sub.data_as<float>();
  double sum = 0;
  for (std::size_t i = 0; i < sub.size(); ++i) sum += p[i];
  EXPECT_NEAR(sum, 1.5224, 1e-2);
}

TEST_F(SampleData, PicksLoadFromJson) {
  auto run = open_sample().get_run("TS_001");
  auto pk = run.get_picks("proteasome", "pytom", "0");
  ASSERT_EQ(pk.size(), 1u);
  const auto& pts = pk[0].points();
  ASSERT_EQ(pts.size(), 10u);
  EXPECT_NEAR(pts[0].location.x, 267.075074, 1e-3);
  EXPECT_NEAR(pts[0].location.y, 357.561490, 1e-3);
  EXPECT_NEAR(pts[0].location.z, 89.847641, 1e-3);
  EXPECT_NEAR(pts[9].location.x, 8.929007, 1e-3);

  // Filtering: 3 proteasome picks (pytom/0, ArtiaX/19, test.user/1234), 2 ribosome
  // (gapstop/0, test.user/1234).
  EXPECT_EQ(run.get_picks("proteasome").size(), 3u);
  EXPECT_EQ(run.get_picks("ribosome").size(), 2u);
}

TEST_F(SampleData, SegmentationsParsed) {
  auto run = open_sample().get_run("TS_001");
  EXPECT_EQ(run.segmentations().size(), 3u);
  auto painting = run.get_segmentations("", "", "painting");
  ASSERT_EQ(painting.size(), 1u);
  EXPECT_TRUE(painting[0].is_multilabel());
  EXPECT_DOUBLE_EQ(painting[0].voxel_size(), 10.0);
  EXPECT_EQ(painting[0].to_array().shape_z(), 64u);

  auto membrane = run.get_segmentations("", "", "membrane");
  ASSERT_EQ(membrane.size(), 1u);
  EXPECT_FALSE(membrane[0].is_multilabel());
  EXPECT_DOUBLE_EQ(membrane[0].voxel_size(), 20.0);
}

#ifdef COPICK_ENABLE_MESH
TEST_F(SampleData, MeshLoadFromGlb) {
  auto run = open_sample().get_run("TS_001");
  auto m = run.get_meshes("membrane", "membrain", "0");
  ASSERT_EQ(m.size(), 1u);
  const copick::Geometry& g = m[0].mesh();  // trimesh-written GLB -> tinygltf
  EXPECT_GT(g.vertices.size(), 0u);
  EXPECT_GT(g.faces.size(), 0u);
}
#endif
