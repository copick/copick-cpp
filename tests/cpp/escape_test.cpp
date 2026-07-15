// Mirrors copick/tests/test_escape.py (the sanitize_name unit tests).
#include <gtest/gtest.h>

#include <string>

#include "copick/errors.h"
#include "copick/escape.h"

using copick::sanitize_name;

TEST(SanitizeName, ValidNamesUnchanged) {
  for (const std::string& name : {"proteasome", "ribosome-70S", "my.object", "TS-001", "abc123"}) {
    bool modified = true;
    EXPECT_EQ(sanitize_name(name, &modified), name) << name;
    EXPECT_FALSE(modified) << name;
    EXPECT_TRUE(copick::is_valid_name(name)) << name;
  }
}

TEST(SanitizeName, EachInvalidCharBecomesDash) {
  // space / \ : * ? " < > | _  each map to a single dash.
  for (const std::string& bad :
       {"a b", "a/b", "a\\b", "a:b", "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b", "a_b"}) {
    EXPECT_EQ(sanitize_name(bad), "a-b") << bad;
    EXPECT_FALSE(copick::is_valid_name(bad)) << bad;
  }
}

TEST(SanitizeName, ConsecutiveInvalidsEachBecomeDash) {
  EXPECT_EQ(sanitize_name("a__b"), "a--b");
  EXPECT_EQ(sanitize_name("a _b"), "a--b");
}

TEST(SanitizeName, TrimsLeadingAndTrailingDashes) {
  EXPECT_EQ(sanitize_name("_abc_"), "abc");
  EXPECT_EQ(sanitize_name("  abc  "), "abc");
  EXPECT_EQ(sanitize_name("__a-b__"), "a-b");
}

TEST(SanitizeName, AllInvalidThrows) {
  EXPECT_THROW(sanitize_name("___"), copick::ValidationError);
  EXPECT_THROW(sanitize_name("   "), copick::ValidationError);
  EXPECT_THROW(sanitize_name(""), copick::ValidationError);
}

TEST(SanitizeName, ModifiedFlag) {
  bool modified = false;
  sanitize_name("a_b", &modified);
  EXPECT_TRUE(modified);
  modified = true;
  sanitize_name("abc", &modified);
  EXPECT_FALSE(modified);
}

TEST(SanitizeName, PreservesUnicodeBytes) {
  // Non-ASCII (UTF-8) bytes are > 0x7F and must pass through unchanged.
  const std::string cafe = "caf\xC3\xA9";  // "café"
  EXPECT_EQ(sanitize_name(cafe), cafe);
}
