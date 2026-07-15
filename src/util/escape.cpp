#include "copick/escape.h"

#include <string>

#include "copick/errors.h"

namespace copick {
namespace {

// Invalid characters: <>:"/\|?* (Windows-reserved), control chars (<= 0x1F and 0x7F),
// ASCII whitespace, and underscore. Mirrors the regex in copick.util.escape.
bool is_invalid_char(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  if (u <= 0x1F || u == 0x7F) return true;
  switch (c) {
    case '<':
    case '>':
    case ':':
    case '"':
    case '/':
    case '\\':
    case '|':
    case '?':
    case '*':
    case '_':
    case ' ':
      return true;
    default:
      return false;
  }
}

}  // namespace

std::string sanitize_name(const std::string& input, bool* was_modified) {
  std::string replaced;
  replaced.reserve(input.size());
  for (char c : input) replaced.push_back(is_invalid_char(c) ? '-' : c);

  const std::size_t begin = replaced.find_first_not_of('-');
  const std::size_t end = replaced.find_last_not_of('-');
  std::string sanitized =
      (begin == std::string::npos) ? std::string() : replaced.substr(begin, end - begin + 1);

  if (sanitized.empty()) {
    throw ValidationError("Filename cannot be empty or completely consist of invalid characters.");
  }
  if (was_modified) *was_modified = (sanitized != input);
  return sanitized;
}

bool is_valid_name(const std::string& input) {
  try {
    bool modified = false;
    sanitize_name(input, &modified);
    return !modified;
  } catch (const ValidationError&) {
    return false;
  }
}

}  // namespace copick
