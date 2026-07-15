// C++11 header guard (see tests/CMakeLists.txt).
//
// Includes every PUBLIC copick header. Compiled with `-std=c++11` by the system
// compiler to prove the headers stay consumable from AreTomo3's C++11 build. Add a
// line here for each new public header under include/copick/.
// Include the umbrella (which pulls in every public header) plus the umbrella itself.
#include "copick/array.h"
#include "copick/config.h"
#include "copick/copick.h"
#include "copick/errors.h"
#include "copick/escape.h"
#include "copick/export.h"
#include "copick/features.h"
#include "copick/fwd.h"
#include "copick/geometry.h"
#include "copick/mesh.h"
#include "copick/object.h"
#include "copick/optional.h"
#include "copick/picks.h"
#include "copick/root.h"
#include "copick/run.h"
#include "copick/segmentation.h"
#include "copick/serialize.h"
#include "copick/tomogram.h"
#include "copick/types.h"
#include "copick/validate.h"
#include "copick/version.h"
#include "copick/voxel_spacing.h"

int main() {
  // Exercise a few constructs to ensure the templates instantiate under C++11.
  copick::PickableObject obj;
  obj.name = "proteasome";
  copick::optional<double> r;
  (void)r.value_or(1.0);
  (void)copick::is_valid_name(obj.name);
  return 0;
}
