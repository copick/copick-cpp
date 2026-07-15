// Backend tests for the Root/Run handles against a real (overlay-only) filesystem
// project. Mirrors parts of copick/tests/test_filesystem.py. Built only under
// COPICK_ENABLE_ZARR.
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "copick/errors.h"
#include "copick/root.h"
#include "copick/run.h"
#include "io/kvstore.hpp"

namespace {

std::string overlay_root(const std::string& name) {
  std::string tmp = ::testing::TempDir();
  if (!tmp.empty() && tmp.back() == '/') tmp.pop_back();
  return "file://" + tmp + "/copick_backend/" + name;
}

std::string config_for(const std::string& root_url) {
  return std::string(R"({"config_type":"filesystem","pickable_objects":[)") +
         R"({"name":"proteasome","is_particle":true,"label":1,"color":[255,0,0,255]})" +
         R"(],"overlay_root":")" + root_url + R"("})";
}

std::vector<std::string> run_names(const copick::Root& root) {
  std::vector<std::string> names;
  for (const auto& r : root.runs()) names.push_back(r.name());
  return names;
}

}  // namespace

TEST(Backend, EmptyProjectHasNoRuns) {
  const std::string url = overlay_root("empty");
  copick::io::KvStore::open(url).remove_prefix("");  // clean
  auto root = copick::from_string(config_for(url));
  EXPECT_TRUE(root.valid());
  EXPECT_TRUE(root.runs().empty());
  EXPECT_FALSE(root.get_run("nope").valid());
  EXPECT_EQ(root.config().pickable_objects.size(), 1u);
}

TEST(Backend, NewRunPersistsSortedAndMarker) {
  const std::string url = overlay_root("newrun");
  auto kv = copick::io::KvStore::open(url);
  kv.remove_prefix("");
  auto root = copick::from_string(config_for(url));

  root.new_run("TS_001");
  root.new_run("TS_003");
  auto r2 = root.new_run("TS_002");
  EXPECT_TRUE(r2.valid());
  EXPECT_EQ(r2.name(), "TS_002");

  // Sorted enumeration.
  EXPECT_EQ(run_names(root), (std::vector<std::string>{"TS_001", "TS_002", "TS_003"}));

  // The overlay marker was written on disk.
  EXPECT_TRUE(kv.exists("ExperimentRuns/TS_001/.meta"));

  // A freshly opened root sees the same runs (persisted, not just cached).
  auto root2 = copick::from_string(config_for(url));
  EXPECT_EQ(run_names(root2), (std::vector<std::string>{"TS_001", "TS_002", "TS_003"}));
}

TEST(Backend, GetRunAndDuplicate) {
  const std::string url = overlay_root("getdup");
  copick::io::KvStore::open(url).remove_prefix("");
  auto root = copick::from_string(config_for(url));

  root.new_run("TS_001");
  EXPECT_TRUE(root.get_run("TS_001").valid());
  EXPECT_FALSE(root.get_run("TS_999").valid());

  EXPECT_THROW(root.new_run("TS_001"), copick::ValidationError);
  auto again = root.new_run("TS_001", /*exist_ok=*/true);
  EXPECT_TRUE(again.valid());
  EXPECT_EQ(again.name(), "TS_001");
}

TEST(Backend, DeleteRun) {
  const std::string url = overlay_root("del");
  auto kv = copick::io::KvStore::open(url);
  kv.remove_prefix("");
  auto root = copick::from_string(config_for(url));

  root.new_run("TS_001");
  root.new_run("TS_002");
  ASSERT_EQ(run_names(root).size(), 2u);

  root.delete_run("TS_001");
  EXPECT_EQ(run_names(root), (std::vector<std::string>{"TS_002"}));
  EXPECT_FALSE(kv.exists("ExperimentRuns/TS_001/.meta"));
  root.delete_run("TS_001");  // no-op, no throw
}

TEST(Backend, NewConfigCreatesProject) {
  std::string tmp = ::testing::TempDir();
  if (!tmp.empty() && tmp.back() == '/') tmp.pop_back();
  const std::string proj = tmp + "/copick_backend/newcfg_proj";
  const std::string cfg = tmp + "/copick_backend/newcfg.json";
  copick::io::KvStore::open("file://" + proj).remove_prefix("");

  auto root = copick::new_config(cfg, "file://" + proj, "my project");
  EXPECT_TRUE(root.valid());
  EXPECT_EQ(root.config().name, "my project");
  root.new_run("TS_010");
  EXPECT_EQ(run_names(root), (std::vector<std::string>{"TS_010"}));
}

TEST(Backend, RejectsNonFilesystemConfig) {
  const char* cfg =
      R"({"config_type":"cryoet_data_portal","pickable_objects":[],"overlay_root":"x"})";
  EXPECT_THROW(copick::from_string(cfg), copick::ValidationError);
}
