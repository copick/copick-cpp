#include <gtest/gtest.h>

#include <string>

#include "copick/version.h"

TEST(Smoke, Version) {
  // The version values are owned by release-please (it bumps the markers in version.h on
  // every release branch), so don't pin a literal here. Check the invariants instead.
  ASSERT_NE(copick::version(), nullptr);
  EXPECT_FALSE(std::string(copick::version()).empty());

  // The runtime string matches the compile-time macro.
  EXPECT_EQ(std::string(copick::version()), std::string(COPICK_VERSION_STRING));

  // COPICK_VERSION_STRING is exactly "MAJOR.MINOR.PATCH" from the numeric markers, so the
  // four release-please markers cannot drift out of sync.
  const std::string composed = std::to_string(COPICK_VERSION_MAJOR) + "." +
                               std::to_string(COPICK_VERSION_MINOR) + "." +
                               std::to_string(COPICK_VERSION_PATCH);
  EXPECT_EQ(std::string(COPICK_VERSION_STRING), composed);
}
