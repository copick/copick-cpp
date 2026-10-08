// Filament objects: a pickable object whose metadata carries a filament spec under
// metadata["copick"]["filament"] (copick >= 1.28). Its picks are points along filaments,
// grouped by instance_id (the filament id, from 1; 0 = unassigned) and ordered along each
// filament.
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_FILAMENT_H
#define COPICK_FILAMENT_H

#include "copick/export.h"
#include "copick/optional.h"
#include "copick/types.h"

namespace copick {

/// A filament object's spec. (copick.models.FilamentSpec)
struct FilamentSpec {
  optional<bool> polar;                ///< the structure has a polarity (microtubules: true)
  optional<double> helical_rise_a;     ///< helical rise in angstrom, > 0 (descriptive)
  optional<double> helical_twist_deg;  ///< helical twist in degrees (descriptive)
};

/// The filament spec of `obj`, or no value when `obj` is not a filament: its metadata has no
/// "copick" object, or that object's "filament" is absent or null. Unknown keys in the spec
/// are ignored. Throws ValidationError when the metadata is not valid JSON, or the spec is
/// not an object of the fields above, has helical_rise_a <= 0, or belongs to an object that
/// is not a particle — the cases copick refuses.
COPICK_API optional<FilamentSpec> filament(const PickableObject& obj);

/// Whether `obj` is a filament object (has a filament spec). Throws as filament().
COPICK_API bool is_filament(const PickableObject& obj);

}  // namespace copick

#endif  // COPICK_FILAMENT_H
