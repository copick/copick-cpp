// zarr.hpp — OME-Zarr array I/O via tensorstore. All chunking/compression/multiscale
// logic lives inside tensorstore's `zarr` (v2) driver; this layer just moves data
// between a copick Array3D and a zarr array level, plus reads/writes the small group
// `.zattrs` (which tensorstore has no driver for).
//
// `kvstore_spec_json` is a tensorstore kvstore spec (JSON string, from KvStore::
// kvstore_spec) addressing the array directory (the one containing `.zarray`).
//
// Compiled at C++20, only under COPICK_ENABLE_ZARR.
#ifndef COPICK_IO_ZARR_HPP
#define COPICK_IO_ZARR_HPP

#include <cstddef>
#include <string>

#include "copick/array.h"

namespace copick {
namespace io {

struct ZarrInfo {
  std::size_t nz = 0;
  std::size_t ny = 0;
  std::size_t nx = 0;
  DType dtype = DType::Float32;
};

struct ZarrWriteOptions {
  // Chunk shape; 0 on an axis means "one chunk spanning the whole axis".
  std::size_t chunk_z = 0;
  std::size_t chunk_y = 0;
  std::size_t chunk_x = 0;
  // Compressor: "blosc" (default; copick/ome-zarr-py compatible), "gzip", "none", or
  // "zstd". NOTE: tensorstore's raw "zstd" frames omit the content-size field and are
  // NOT readable by numcodecs < 0.16.2 (which zarr 2.x, and thus copick, pins) — so it
  // is not the default. blosc self-describes its size and is universally readable.
  std::string compressor = "blosc";
  int compression_level = 5;
};

/// Read shape + dtype of a zarr v2 array (metadata only).
ZarrInfo zarr_info(const std::string& kvstore_spec_json);

/// Read a region (default: whole array) of a zarr v2 array into an Array3D (z, y, x).
Array3D zarr_read(const std::string& kvstore_spec_json, const Region& region = Region());

/// Create (overwriting any existing) a zarr v2 array from an Array3D.
void zarr_write(const std::string& kvstore_spec_json, const Array3D& data,
                const ZarrWriteOptions& opts = ZarrWriteOptions());

/// Write `data` into a region of an existing zarr v2 array. `data` shape must match the
/// region extent.
void zarr_write_region(const std::string& kvstore_spec_json, const Array3D& data,
                       const Region& region);

}  // namespace io
}  // namespace copick

#endif  // COPICK_IO_ZARR_HPP
