// Example / reverse parity check: read a copick project (here, one written by Python
// copick) with copick-cpp and verify the data.
//
//   read_project <config.json>
#include <cstdint>
#include <iostream>
#include <string>

#include "copick/copick.h"

static int fail(const std::string& msg) {
  std::cerr << "MISMATCH: " << msg << "\n";
  return 1;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: read_project <config.json>\n";
    return 2;
  }
  copick::Root root = copick::from_file(argv[1]);

  copick::Run run = root.get_run("TS_PY");
  if (!run.valid()) return fail("run TS_PY not found");

  // Tomogram: 4x4x4 float32 ramp (written by Python, blosc-compressed, multiscale).
  copick::Tomogram tomo = run.get_voxel_spacing(10.0).get_tomogram("wbp");
  if (!tomo.valid()) return fail("tomogram wbp not found");
  copick::Array3D vol = tomo.to_array();
  if (vol.shape_z() != 4 || vol.shape_x() != 4) return fail("tomogram shape");
  if (vol.data_as<float>()[0] != 0.0f || vol.data_as<float>()[63] != 63.0f)
    return fail("tomogram values");

  // Segmentation.
  auto segs = run.get_segmentations("py", "1", "proteasome");
  if (segs.size() != 1) return fail("expected 1 segmentation");
  copick::Array3D mask = segs[0].to_array();
  if (mask.size() != 64 || mask.dtype() != copick::DType::UInt8)
    return fail("segmentation shape/dtype");

  // Picks.
  auto picks = run.get_picks("proteasome");
  if (picks.size() != 1) return fail("expected 1 pick set");
  const auto& pts = picks[0].points();
  if (pts.size() != 2) return fail("expected 2 points");
  if (pts[0].location.x != 1.0 || pts[1].location.z != 13.0) return fail("pick locations");

  std::cout << "READ OK — copick-cpp read the Python-written project correctly.\n";
  return 0;
}
