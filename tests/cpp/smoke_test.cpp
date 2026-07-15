#include <gtest/gtest.h>

#include <string>

#include "copick/version.h"

TEST(Smoke, Version) {
  EXPECT_STREQ(copick::version(), "0.1.0");
  EXPECT_EQ(std::string(copick::version()), std::string(COPICK_VERSION_STRING));
}
