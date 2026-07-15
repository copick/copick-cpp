// Mesh tests: GLB round-trip via tinygltf. Built only under COPICK_ENABLE_ZARR (and
// exercises tinygltf when COPICK_ENABLE_MESH is on, which the `full` preset enables).
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "copick/errors.h"
#include "copick/geometry.h"
#include "copick/mesh.h"
#include "copick/root.h"
#include "copick/run.h"
#include "io/kvstore.hpp"

namespace {

std::string overlay_root(const std::string& name) {
  std::string tmp = ::testing::TempDir();
  if (!tmp.empty() && tmp.back() == '/') tmp.pop_back();
  return "file://" + tmp + "/copick_mesh/" + name;
}

std::string config_for(const std::string& root_url) {
  return std::string(R"({"config_type":"filesystem","user_id":"alice","pickable_objects":[)") +
         R"({"name":"membrane","is_particle":false,"label":1,"color":[0,0,0,255]})" +
         R"(],"overlay_root":")" + root_url + R"("})";
}

copick::Geometry make_tetrahedron() {
  copick::Geometry g;
  g.vertices = {{{0.f, 0.f, 0.f}}, {{10.f, 0.f, 0.f}}, {{0.f, 10.f, 0.f}}, {{0.f, 0.f, 10.f}}};
  g.faces = {{{0, 1, 2}}, {{0, 1, 3}}, {{0, 2, 3}}, {{1, 2, 3}}};
  return g;
}

}  // namespace

TEST(MeshTest, GlbRoundTripAndEnumerate) {
  const std::string url = overlay_root("roundtrip");
  copick::io::KvStore::open(url).remove_prefix("");
  auto root = copick::from_string(config_for(url));
  auto run = root.new_run("TS_001");

  auto mesh = run.new_mesh("membrane", "0");  // user defaults to alice
  EXPECT_EQ(mesh.object_name(), "membrane");
  EXPECT_EQ(mesh.user_id(), "alice");
  EXPECT_TRUE(mesh.from_tool());

  mesh.set_mesh(make_tetrahedron());
  mesh.store();

  // On-disk filename convention.
  EXPECT_TRUE(
      copick::io::KvStore::open(url).exists("ExperimentRuns/TS_001/Meshes/alice_0_membrane.glb"));

  // Reload from a fresh root and verify the geometry survived the GLB round-trip.
  auto r2 = copick::from_string(config_for(url)).get_run("TS_001");
  auto found = r2.get_meshes("membrane");
  ASSERT_EQ(found.size(), 1u);
  const copick::Geometry& g = found[0].mesh();
  ASSERT_EQ(g.vertices.size(), 4u);
  ASSERT_EQ(g.faces.size(), 4u);
  EXPECT_FLOAT_EQ(g.vertices[1][0], 10.0f);
  EXPECT_FLOAT_EQ(g.vertices[3][2], 10.0f);
  EXPECT_EQ(g.faces[3][0], 1u);
  EXPECT_EQ(g.faces[3][2], 3u);
}

TEST(MeshTest, NewMeshValidationAndFilter) {
  const std::string url = overlay_root("valid");
  copick::io::KvStore::open(url).remove_prefix("");
  auto run = copick::from_string(config_for(url)).new_run("TS_001");

  EXPECT_THROW(run.new_mesh("unknown", "0"), copick::ValidationError);
  run.new_mesh("membrane", "0");
  EXPECT_THROW(run.new_mesh("membrane", "0"), copick::ValidationError);
  EXPECT_TRUE(run.new_mesh("membrane", "0", "", true).valid());

  EXPECT_EQ(run.get_meshes("membrane").size(), 1u);
  EXPECT_EQ(run.get_meshes("", "bob").size(), 0u);
}
