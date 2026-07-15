// A single process-wide tensorstore Context.
//
// tensorstore's Context owns the cache pool, thread pools and file I/O concurrency
// resources. Using a *fresh* Context::Default() per kvstore and per zarr operation gives
// each its own cache pool; concurrent writes to the same file tree through independent
// cache pools are not coordinated and clobber each other. All copick-cpp I/O therefore
// shares this one context.
//
// Compiled at C++20, only under COPICK_ENABLE_ZARR.
#ifndef COPICK_IO_CONTEXT_HPP
#define COPICK_IO_CONTEXT_HPP

#include <tensorstore/context.h>

namespace copick {
namespace io {

/// The shared process-wide tensorstore Context (returns a refcounted copy).
tensorstore::Context shared_context();

}  // namespace io
}  // namespace copick

#endif  // COPICK_IO_CONTEXT_HPP
