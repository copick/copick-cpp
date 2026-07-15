// JSON (de)serialization against copick's on-disk format.
#include <gtest/gtest.h>

#include <string>

#include "copick/config.h"
#include "copick/errors.h"
#include "copick/serialize.h"
#include "copick/types.h"

// A config in copick's filesystem format (from the sample project / README).
static const char* kSampleConfig = R"JSON({
  "name": "test",
  "description": "A test project.",
  "version": "1.0.0",
  "pickable_objects": [
    {"name": "proteasome", "is_particle": true, "pdb_id": "3J9I", "label": 1,
     "color": [255, 0, 0, 255], "radius": 60, "map_threshold": 0.0418},
    {"name": "ribosome", "is_particle": true, "pdb_id": "7P6Z", "label": 2,
     "color": [0, 255, 0, 255], "radius": 150, "map_threshold": 0.037},
    {"name": "membrane", "is_particle": false, "label": 3, "color": [0, 0, 0, 255]}
  ],
  "overlay_root": "local:///tmp/sample_project",
  "overlay_fs_args": {"auto_mkdir": true}
})JSON";

TEST(ConfigJson, ParsesSampleConfig) {
  copick::CopickConfig cfg = copick::config_from_json(kSampleConfig);
  EXPECT_EQ(cfg.name, "test");
  EXPECT_EQ(cfg.version, "1.0.0");
  EXPECT_EQ(cfg.config_type, "filesystem");  // defaulted when absent
  ASSERT_EQ(cfg.pickable_objects.size(), 3u);

  const copick::PickableObject& prot = cfg.pickable_objects[0];
  EXPECT_EQ(prot.name, "proteasome");
  EXPECT_TRUE(prot.is_particle);
  EXPECT_EQ(prot.label, 1);
  EXPECT_EQ(prot.color, (copick::Color{{255, 0, 0, 255}}));
  ASSERT_TRUE(prot.pdb_id.has_value());
  EXPECT_EQ(*prot.pdb_id, "3J9I");
  ASSERT_TRUE(prot.radius.has_value());
  EXPECT_DOUBLE_EQ(*prot.radius, 60.0);

  EXPECT_FALSE(cfg.pickable_objects[2].is_particle);  // membrane
  EXPECT_EQ(cfg.overlay_root, "local:///tmp/sample_project");
  EXPECT_FALSE(cfg.static_root.has_value());
}

TEST(ConfigJson, RoundTrip) {
  copick::CopickConfig cfg = copick::config_from_json(kSampleConfig);
  const std::string dumped = copick::config_to_json(cfg);
  copick::CopickConfig again = copick::config_from_json(dumped);

  ASSERT_EQ(again.pickable_objects.size(), cfg.pickable_objects.size());
  for (std::size_t i = 0; i < cfg.pickable_objects.size(); ++i) {
    EXPECT_EQ(again.pickable_objects[i].name, cfg.pickable_objects[i].name);
    EXPECT_EQ(again.pickable_objects[i].label, cfg.pickable_objects[i].label);
    EXPECT_EQ(again.pickable_objects[i].color, cfg.pickable_objects[i].color);
  }
  EXPECT_EQ(again.overlay_root, cfg.overlay_root);
  EXPECT_EQ(again.name, cfg.name);
}

TEST(ConfigJson, InvalidNameRejected) {
  const char* bad =
      R"JSON({"pickable_objects":[{"name":"bad_name","is_particle":true,"label":1}]})JSON";
  EXPECT_THROW(copick::config_from_json(bad), copick::ValidationError);
}

TEST(ConfigJson, MetadataPreserved) {
  const char* with_meta = R"JSON({"pickable_objects":[
      {"name":"prot","is_particle":true,"label":1,
       "metadata":{"source":"exp","confidence":0.95,"nested":{"k":[1,2,3]}}}]})JSON";
  copick::CopickConfig cfg = copick::config_from_json(with_meta);
  const std::string& meta = cfg.pickable_objects[0].metadata;
  // Round-trips through serialization; nested content preserved.
  copick::CopickConfig again = copick::config_from_json(copick::config_to_json(cfg));
  EXPECT_NE(meta.find("confidence"), std::string::npos);
  EXPECT_NE(again.pickable_objects[0].metadata.find("nested"), std::string::npos);
}

// A picks file in copick's on-disk format (note the `transformation_` key and the
// null run_name, exactly as pydantic model_dump() writes them).
static const char* kSamplePicks = R"JSON({
  "pickable_object_name": "ribosome",
  "user_id": "gapstop",
  "session_id": "0",
  "run_name": null,
  "voxel_spacing": null,
  "unit": "angstrom",
  "trust_orientation": true,
  "points": [
    {"location": {"x": 100.0, "y": 200.0, "z": 300.0},
     "transformation_": [[1,0,0,10],[0,1,0,20],[0,0,1,30],[0,0,0,1]],
     "instance_id": 5, "score": 0.9}
  ]
})JSON";

TEST(PicksJson, ParsesCopickFormat) {
  copick::CopickPicksFile f = copick::picks_from_json(kSamplePicks);
  EXPECT_EQ(f.pickable_object_name, "ribosome");
  EXPECT_EQ(f.user_id, "gapstop");
  EXPECT_EQ(f.session_id, "0");
  EXPECT_FALSE(f.run_name.has_value());  // explicit null -> unset
  EXPECT_TRUE(f.trust_orientation);
  ASSERT_EQ(f.points.size(), 1u);

  const copick::Point& p = f.points[0];
  EXPECT_DOUBLE_EQ(p.location.x, 100.0);
  EXPECT_DOUBLE_EQ(p.location.z, 300.0);
  EXPECT_EQ(p.instance_id, 5);
  EXPECT_DOUBLE_EQ(p.score, 0.9);
  EXPECT_DOUBLE_EQ(p.transformation[0][3], 10.0);
  EXPECT_DOUBLE_EQ(p.transformation[2][3], 30.0);
}

TEST(PicksJson, RoundTripKeepsTransformationKey) {
  copick::CopickPicksFile f = copick::picks_from_json(kSamplePicks);
  const std::string dumped = copick::picks_to_json(f);
  // On-disk key must be `transformation_` (matches pydantic), so Python copick can read it.
  EXPECT_NE(dumped.find("transformation_"), std::string::npos);
  EXPECT_EQ(dumped.find("\"transformation\":"), std::string::npos);

  copick::CopickPicksFile again = copick::picks_from_json(dumped);
  ASSERT_EQ(again.points.size(), 1u);
  EXPECT_DOUBLE_EQ(again.points[0].location.y, 200.0);
  EXPECT_DOUBLE_EQ(again.points[0].transformation[1][3], 20.0);
}
