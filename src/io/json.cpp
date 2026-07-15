#include "copick/serialize.h"

#include <rfl/json.hpp>

#include <string>

#include "copick/errors.h"
#include "copick/validate.h"
#include "io/dto.hpp"

namespace copick {

CopickConfig config_from_json(const std::string& json) {
  const auto result = rfl::json::read<io::ConfigDTO>(json);
  if (!result) {
    throw ValidationError("Failed to parse copick config JSON: " + result.error().what());
  }
  CopickConfig config = io::from_dto(result.value());
  validate(config);
  return config;
}

std::string config_to_json(const CopickConfig& config, bool pretty) {
  const io::ConfigDTO dto = io::to_dto(config);
  return pretty ? rfl::json::write(dto, rfl::json::pretty) : rfl::json::write(dto);
}

CopickPicksFile picks_from_json(const std::string& json) {
  const auto result = rfl::json::read<io::PicksFileDTO>(json);
  if (!result) {
    throw ValidationError("Failed to parse copick picks JSON: " + result.error().what());
  }
  return io::from_dto(result.value());
}

std::string picks_to_json(const CopickPicksFile& file, bool pretty) {
  const io::PicksFileDTO dto = io::to_dto(file);
  return pretty ? rfl::json::write(dto, rfl::json::pretty) : rfl::json::write(dto);
}

}  // namespace copick
