// Two-source overlay tests: a read-only static source merged with a writable overlay.
// Built only under COPICK_ENABLE_ZARR.
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "copick/array.h"
#include "copick/errors.h"
#include "copick/picks.h"
#include "copick/root.h"
#include "copick/run.h"
#include "copick/tomogram.h"
#include "copick/types.h"
#include "copick/voxel_spacing.h"
#include "io/kvstore.hpp"

namespace {

std::string base_dir(const std::string& name) {
  std::string tmp = ::testing::TempDir();
  if (!tmp.empty() && tmp.back() == '/') tmp.pop_back();
  return tmp + "/copick_overlay/" + name;
}

std::string objects() {
  return R"([{"name":"proteasome","is_particle":true,"label":1,"color":[255,0,0,255]}])";
}

std::string single_source_config(const std::string& overlay) {
  return std::string(R"({"config_type":"filesystem","user_id":"alice","pickable_objects":)") +
         objects() + R"(,"overlay_root":")" + overlay + R"("})";
}

std::string two_source_config(const std::string& static_root, const std::string& overlay) {
  return std::string(R"({"config_type":"filesystem","user_id":"alice","pickable_objects":)") +
         objects() + R"(,"static_root":")" + static_root + R"(","overlay_root":")" + overlay +
         R"("})";
}

copick::Array3D block(float v) {
  copick::Array3D a(copick::DType::Float32, 2, 2, 2);
  for (std::size_t i = 0; i < a.size(); ++i) a.data_as<float>()[i] = v;
  return a;
}

std::vector<std::string> names(const copick::Root& r) {
  std::vector<std::string> n;
  for (const auto& run : r.runs()) n.push_back(run.name());
  std::sort(n.begin(), n.end());
  return n;
}

}  // namespace

TEST(OverlayTest, StaticIsReadOnlyOverlayIsWritable) {
  const std::string static_url = "file://" + base_dir("static");
  const std::string overlay_url = "file://" + base_dir("overlay");
  copick::io::KvStore::open(static_url).remove_prefix("");
  copick::io::KvStore::open(overlay_url).remove_prefix("");

  // Seed the static source (as a standalone project).
  {
    auto seed = copick::from_string(single_source_config(static_url));
    auto run = seed.new_run("TS_STATIC");
    run.new_voxel_spacing(10.0).new_tomogram("wbp").from_numpy(block(7.0f));
    auto pk = run.new_picks("proteasome", "0");
    copick::Point p;
    p.location.x = 1.0;
    pk.set_points({p});
    pk.store();
  }

  // Open as a two-source project.
  auto root = copick::from_string(two_source_config(static_url, overlay_url));

  // The static run is visible.
  auto run = root.get_run("TS_STATIC");
  ASSERT_TRUE(run.valid());

  // Its tomogram reads (from static) but is read-only.
  auto tomo = run.get_voxel_spacing(10.0).get_tomogram("wbp");
  ASSERT_TRUE(tomo.valid());
  EXPECT_FLOAT_EQ(tomo.numpy().data_as<float>()[0], 7.0f);
  EXPECT_THROW(tomo.from_numpy(block(1.0f)), copick::PermissionError);

  // Its picks read (from static) but are read-only.
  auto picks = run.get_picks("proteasome");
  ASSERT_EQ(picks.size(), 1u);
  EXPECT_DOUBLE_EQ(picks[0].points()[0].location.x, 1.0);
  EXPECT_THROW(picks[0].store(), copick::PermissionError);

  // Writes land in the overlay; the static source is untouched.
  root.new_run("TS_OVERLAY");
  EXPECT_EQ(names(root), (std::vector<std::string>{"TS_OVERLAY", "TS_STATIC"}));
  EXPECT_FALSE(copick::io::KvStore::open(static_url).exists("ExperimentRuns/TS_OVERLAY/.meta"));
  EXPECT_TRUE(copick::io::KvStore::open(overlay_url).exists("ExperimentRuns/TS_OVERLAY/.meta"));

  // A brand-new tomogram in the overlay is writable and round-trips.
  auto wtomo = root.get_run("TS_OVERLAY").new_voxel_spacing(10.0).new_tomogram("wbp");
  wtomo.from_numpy(block(3.0f));
  EXPECT_FLOAT_EQ(wtomo.numpy().data_as<float>()[0], 3.0f);
}
