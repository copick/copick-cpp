// Mirrors the model-validation parts of copick/tests/test_object_models.py and
// test_escape.py: PickableObject / CopickConfig validation rules.
#include <gtest/gtest.h>

#include "copick/config.h"
#include "copick/errors.h"
#include "copick/filament.h"
#include "copick/types.h"
#include "copick/validate.h"

namespace {

copick::PickableObject make_valid() {
  copick::PickableObject obj;
  obj.name = "proteasome";
  obj.is_particle = true;
  obj.label = 1;
  obj.color = {{255, 0, 0, 255}};
  return obj;
}

}  // namespace

TEST(PickableObject, ValidPasses) {
  EXPECT_NO_THROW(copick::validate(make_valid()));
}

TEST(PickableObject, DefaultsMatchCopick) {
  copick::PickableObject obj;
  obj.name = "x";
  EXPECT_EQ(obj.label, 1);
  EXPECT_EQ(obj.color, (copick::Color{{100, 100, 100, 255}}));
  EXPECT_EQ(obj.metadata, "{}");
  EXPECT_FALSE(obj.emdb_id.has_value());
}

TEST(PickableObject, Label0Reserved) {
  copick::PickableObject obj = make_valid();
  obj.label = 0;
  EXPECT_THROW(copick::validate(obj), copick::ValidationError);
}

TEST(PickableObject, ColorOutOfRangeThrows) {
  copick::PickableObject obj = make_valid();
  obj.color = {{256, 0, 0, 255}};
  EXPECT_THROW(copick::validate(obj), copick::ValidationError);
  obj.color = {{-1, 0, 0, 255}};
  EXPECT_THROW(copick::validate(obj), copick::ValidationError);
}

TEST(PickableObject, InvalidNameThrows) {
  copick::PickableObject obj = make_valid();
  obj.name = "bad_name";  // underscore is invalid
  EXPECT_THROW(copick::validate(obj), copick::ValidationError);
  obj.name = "bad name";
  EXPECT_THROW(copick::validate(obj), copick::ValidationError);
}

TEST(PickableObject, ValidNameCharsPass) {
  copick::PickableObject obj = make_valid();
  obj.name = "good.name-123";
  EXPECT_NO_THROW(copick::validate(obj));
}

TEST(Config, DuplicateNameThrows) {
  copick::CopickConfig cfg;
  copick::PickableObject a = make_valid();
  a.name = "dup";
  a.label = 1;
  copick::PickableObject b = make_valid();
  b.name = "dup";
  b.label = 2;
  cfg.pickable_objects = {a, b};
  EXPECT_THROW(copick::validate(cfg), copick::ValidationError);
}

TEST(Config, DuplicateLabelThrows) {
  copick::CopickConfig cfg;
  copick::PickableObject a = make_valid();
  a.name = "one";
  a.label = 5;
  copick::PickableObject b = make_valid();
  b.name = "two";
  b.label = 5;
  cfg.pickable_objects = {a, b};
  EXPECT_THROW(copick::validate(cfg), copick::ValidationError);
}

TEST(Point, LastRowValidation) {
  copick::Point p;  // identity default is valid
  EXPECT_NO_THROW(copick::validate(p));
  p.transformation[3] = {{1.0, 0.0, 0.0, 1.0}};
  EXPECT_THROW(copick::validate(p), copick::ValidationError);
}

// --- Filament spec (metadata["copick"]["filament"]) -----------------------------

TEST(Filament, SpecIsReadFromMetadata) {
  copick::PickableObject obj = make_valid();
  obj.metadata =
      R"({"copick": {"filament": {"polar": true, "helical_rise_a": 82, "future_key": 1}}, "other": 2})";
  const copick::optional<copick::FilamentSpec> spec = copick::filament(obj);
  ASSERT_TRUE(spec.has_value());
  const copick::FilamentSpec& s = *spec;
  EXPECT_TRUE(s.polar.has_value() && *s.polar);
  EXPECT_DOUBLE_EQ(*s.helical_rise_a, 82.0);
  EXPECT_FALSE(s.helical_twist_deg.has_value());
  EXPECT_TRUE(copick::is_filament(obj));

  obj.metadata = R"({"copick": {"filament": {}}})";
  EXPECT_TRUE(copick::is_filament(obj));
}

TEST(Filament, AbsentNullOrForeignNamespaceIsNotAFilament) {
  copick::PickableObject obj = make_valid();
  for (const char* metadata :
       {"{}", "", R"({"copick": {}})", R"({"copick": {"filament": null}})", R"({"copick": null})",
        R"({"copick": "not ours"})", R"({"copick": [1, 2]})"}) {
    obj.metadata = metadata;
    EXPECT_FALSE(copick::is_filament(obj)) << metadata;
  }
}

TEST(Filament, InvalidSpecThrows) {
  copick::PickableObject obj = make_valid();
  for (const char* metadata :
       {R"({"copick": {"filament": true}})", R"({"copick": {"filament": {"polar": "yes"}}})",
        R"({"copick": {"filament": {"helical_rise_a": 0}}})", "not json"}) {
    obj.metadata = metadata;
    EXPECT_THROW(copick::filament(obj), copick::ValidationError) << metadata;
  }
  obj.metadata = R"({"copick": {"filament": {}}})";
  obj.is_particle = false;
  EXPECT_THROW(copick::filament(obj), copick::ValidationError);
}
