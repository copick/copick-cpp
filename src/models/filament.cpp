#include "copick/filament.h"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <string>

#include "copick/errors.h"

namespace copick {
namespace {

// The namespace and the spec are read as generic JSON first, so a "copick" value that is not
// an object (which copick leaves alone) is told apart from a malformed spec (which it refuses).
struct MetadataDTO {
  std::optional<rfl::Generic> copick;
};

struct NamespaceDTO {
  std::optional<rfl::Generic> filament;
};

struct FilamentSpecDTO {
  std::optional<bool> polar;
  std::optional<double> helical_rise_a;
  std::optional<double> helical_twist_deg;
};

}  // namespace

optional<FilamentSpec> filament(const PickableObject& obj) {
  const std::string metadata = obj.metadata.empty() ? "{}" : obj.metadata;
  const auto meta = rfl::json::read<MetadataDTO>(metadata);
  if (!meta) {
    throw ValidationError("Invalid metadata of object '" + obj.name + "': " + meta.error().what());
  }
  if (!meta.value().copick) return optional<FilamentSpec>();
  const auto ns = rfl::json::read<NamespaceDTO>(rfl::json::write(*meta.value().copick));
  if (!ns || !ns.value().filament) return optional<FilamentSpec>();

  const std::string spec_json = rfl::json::write(*ns.value().filament);
  if (spec_json == "null") return optional<FilamentSpec>();
  const auto spec = rfl::json::read<FilamentSpecDTO>(spec_json);
  if (!spec) {
    throw ValidationError("Invalid filament spec of object '" + obj.name +
                          "': " + spec.error().what());
  }
  if (!obj.is_particle) {
    throw ValidationError("Object '" + obj.name +
                          "' has a filament spec but is not a particle (is_particle=false).");
  }
  const FilamentSpecDTO& d = spec.value();
  if (d.helical_rise_a && !(*d.helical_rise_a > 0.0)) {
    throw ValidationError("Filament spec of object '" + obj.name +
                          "': helical_rise_a must be > 0.");
  }

  FilamentSpec out;
  if (d.polar) out.polar = *d.polar;
  if (d.helical_rise_a) out.helical_rise_a = *d.helical_rise_a;
  if (d.helical_twist_deg) out.helical_twist_deg = *d.helical_twist_deg;
  return out;
}

bool is_filament(const PickableObject& obj) {
  return filament(obj).has_value();
}

}  // namespace copick
