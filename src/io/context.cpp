#include "io/context.hpp"

namespace copick {
namespace io {

tensorstore::Context shared_context() {
  static tensorstore::Context ctx = tensorstore::Context::Default();
  return ctx;
}

}  // namespace io
}  // namespace copick
