// OME-NGFF group metadata helpers. tensorstore's zarr driver writes each array level's
// `.zarray`, but there is no OME driver, so the group-level `.zgroup` and the multiscales
// `.zattrs` are ours to write/read. This is metadata only — no chunk/compression logic.
//
// Compiled at C++20, only under COPICK_ENABLE_ZARR.
#ifndef COPICK_IO_OME_HPP
#define COPICK_IO_OME_HPP

#include <string>

namespace copick {
namespace io {

/// Contents of a Zarr v2 group `.zgroup` file.
std::string zgroup_json();

/// OME-NGFF multiscales `.zattrs` for `levels` pyramid levels, base voxel size in
/// angstrom (each level's scale doubles). Axes are z, y, x (space, angstrom).
std::string ome_multiscales_json(double voxel_size, int levels);

/// Parse the base voxel size (in angstrom) from a group `.zattrs` multiscales document,
/// applying axis-unit conversion. Mirrors copick.util.ome.get_voxel_size_from_zarr.
double voxel_size_from_zattrs(const std::string& zattrs_json);

}  // namespace io
}  // namespace copick

#endif  // COPICK_IO_OME_HPP
