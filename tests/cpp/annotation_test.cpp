// Annotation + object tests: Picks (JSON), Segmentation (zarr), Object (density maps).
// Built only under COPICK_ENABLE_ZARR.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "copick/array.h"
#include "copick/errors.h"
#include "copick/object.h"
#include "copick/picks.h"
#include "copick/root.h"
#include "copick/run.h"
#include "copick/segmentation.h"
#include "copick/types.h"
#include "io/kvstore.hpp"

namespace {

std::string overlay_root(const std::string& name) {
  std::string tmp = ::testing::TempDir();
  if (!tmp.empty() && tmp.back() == '/') tmp.pop_back();
  return "file://" + tmp + "/copick_annot/" + name;
}

std::string config_for(const std::string& root_url) {
  return std::string(R"({"config_type":"filesystem","user_id":"alice","pickable_objects":[)") +
         R"({"name":"proteasome","is_particle":true,"label":1,"color":[255,0,0,255]},)" +
         R"({"name":"membrane","is_particle":false,"label":2,"color":[0,0,0,255]})" +
         R"(],"overlay_root":")" + root_url + R"("})";
}

copick::Point make_point(double x, double y, double z, int instance) {
  copick::Point p;
  p.location.x = x;
  p.location.y = y;
  p.location.z = z;
  p.instance_id = instance;
  p.transformation[0][3] = x;  // arbitrary translation
  return p;
}

}  // namespace

class AnnotationTest : public ::testing::Test {
 protected:
  std::string url;
  copick::Root root;
  copick::Run run;
  void SetUp() override {
    url = overlay_root(::testing::UnitTest::GetInstance()->current_test_info()->name());
    copick::io::KvStore::open(url).remove_prefix("");
    root = copick::from_string(config_for(url));
    run = root.new_run("TS_001");
  }
};

TEST_F(AnnotationTest, ObjectsFromConfig) {
  EXPECT_EQ(root.pickable_objects().size(), 2u);
  auto prot = root.get_object("proteasome");
  ASSERT_TRUE(prot.valid());
  EXPECT_TRUE(prot.is_particle());
  EXPECT_EQ(prot.label(), 1);
  EXPECT_EQ(prot.color(), (copick::Color{{255, 0, 0, 255}}));
  EXPECT_FALSE(root.get_object("membrane").is_particle());
  EXPECT_FALSE(root.get_object("nope").valid());
}

TEST_F(AnnotationTest, ObjectDensityMap) {
  auto prot = root.get_object("proteasome");
  EXPECT_FALSE(prot.has_density());
  copick::Array3D vol(copick::DType::Float32, 2, 2, 2);
  for (std::size_t i = 0; i < vol.size(); ++i) vol.data_as<float>()[i] = static_cast<float>(i);
  prot.from_numpy(vol, 10.0);
  EXPECT_TRUE(prot.has_density());
  EXPECT_FLOAT_EQ(prot.numpy().data_as<float>()[7], 7.0f);

  // Non-particle objects reject density writes.
  EXPECT_THROW(root.get_object("membrane").from_numpy(vol, 10.0), copick::ValidationError);
}

TEST_F(AnnotationTest, PicksRoundTripAndFilter) {
  auto picks = run.new_picks("proteasome", "0");  // user defaults to "alice"
  EXPECT_EQ(picks.object_name(), "proteasome");
  EXPECT_EQ(picks.user_id(), "alice");
  EXPECT_EQ(picks.session_id(), "0");
  EXPECT_TRUE(picks.from_tool());

  std::vector<copick::Point> pts = {make_point(1, 2, 3, 0), make_point(4, 5, 6, 1)};
  picks.set_points(pts);
  picks.store();

  // On-disk filename convention.
  EXPECT_TRUE(
      copick::io::KvStore::open(url).exists("ExperimentRuns/TS_001/Picks/alice_0_proteasome.json"));

  // Reload from a fresh root.
  auto root2 = copick::from_string(config_for(url));
  auto r2 = root2.get_run("TS_001");
  auto found = r2.get_picks("proteasome");
  ASSERT_EQ(found.size(), 1u);
  const auto& loaded = found[0].points();
  ASSERT_EQ(loaded.size(), 2u);
  EXPECT_DOUBLE_EQ(loaded[0].location.x, 1.0);
  EXPECT_DOUBLE_EQ(loaded[1].location.z, 6.0);
  EXPECT_DOUBLE_EQ(loaded[1].transformation[0][3], 4.0);

  // Filters.
  EXPECT_EQ(r2.get_picks("ribosome").size(), 0u);
  EXPECT_EQ(r2.get_picks("", "alice", "0").size(), 1u);
  EXPECT_EQ(r2.get_picks("", "bob").size(), 0u);
}

TEST_F(AnnotationTest, NewPicksValidation) {
  EXPECT_THROW(run.new_picks("unknown", "0"), copick::ValidationError);  // not pickable
  run.new_picks("proteasome", "0");
  EXPECT_THROW(run.new_picks("proteasome", "0"), copick::ValidationError);  // duplicate
  EXPECT_TRUE(run.new_picks("proteasome", "0", "", true).valid());          // exist_ok
}

TEST_F(AnnotationTest, SegmentationRoundTrip) {
  auto seg = run.new_segmentation(10.0, "proteasome", "1", /*is_multilabel=*/false, "alice");
  EXPECT_EQ(seg.name(), "proteasome");
  EXPECT_FALSE(seg.is_multilabel());
  EXPECT_DOUBLE_EQ(seg.voxel_size(), 10.0);
  EXPECT_TRUE(seg.from_user());

  copick::Array3D mask(copick::DType::UInt8, 3, 3, 3);
  for (std::size_t i = 0; i < mask.size(); ++i) mask.data_as<std::uint8_t>()[i] = i % 2;
  seg.from_numpy(mask);

  // Multilabel with an arbitrary (non-pickable) name is allowed.
  auto ml = run.new_segmentation(10.0, "painting", "1", /*is_multilabel=*/true, "alice");
  copick::Array3D lab(copick::DType::UInt16, 2, 2, 2);
  ml.from_numpy(lab);

  // On-disk filenames.
  auto kv = copick::io::KvStore::open(url);
  EXPECT_TRUE(
      kv.exists("ExperimentRuns/TS_001/Segmentations/10.000_alice_1_proteasome.zarr/.zgroup"));
  EXPECT_TRUE(kv.exists(
      "ExperimentRuns/TS_001/Segmentations/10.000_alice_1_painting-multilabel.zarr/.zgroup"));

  // Enumeration + filters (from a fresh root, so it comes off disk).
  auto r2 = copick::from_string(config_for(url)).get_run("TS_001");
  ASSERT_EQ(r2.segmentations().size(), 2u);
  auto prot = r2.get_segmentations("", "", "proteasome");
  ASSERT_EQ(prot.size(), 1u);
  EXPECT_FALSE(prot[0].is_multilabel());
  auto paint = r2.get_segmentations("", "", "painting");
  ASSERT_EQ(paint.size(), 1u);
  EXPECT_TRUE(paint[0].is_multilabel());
  EXPECT_EQ(paint[0].numpy().size(), 8u);

  // Data round-trips.
  auto back = prot[0].numpy();
  EXPECT_EQ(back.size(), 27u);
  EXPECT_EQ(back.dtype(), copick::DType::UInt8);
}
