// Example: open an existing copick project, read data from it, and write new derived data
// back. This is the common workflow — e.g. process a published (read-only static) dataset
// and record results into the writable overlay.
//
//   modify_project <config.json>
//
// Reads run TS_001's "wbp" tomogram, thresholds it into a segmentation, records a pick at
// the brightest voxel, and appends a new run — all idempotently (exist_ok = true).
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "copick/copick.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: modify_project <config.json>\n";
    return 2;
  }
  copick::Root root = copick::from_file(argv[1]);

  copick::Run run = root.get_run("TS_001");
  if (!run.valid()) {
    std::cerr << "run TS_001 not found\n";
    return 1;
  }

  // --- Read existing data --------------------------------------------------------
  copick::VoxelSpacing vs = run.get_voxel_spacing(10.0);
  copick::Tomogram tomo = vs.get_tomogram("wbp");
  if (!tomo.valid()) {
    std::cerr << "tomogram wbp not found\n";
    return 1;
  }
  copick::Array3D vol = tomo.to_array();  // float32 (z, y, x)
  const float* src = vol.data_as<float>();

  // --- Derive new data -----------------------------------------------------------
  // Threshold into a binary mask and find the brightest voxel.
  const float threshold = 30.0f;
  copick::Array3D mask(copick::DType::UInt8, vol.shape_z(), vol.shape_y(), vol.shape_x());
  std::uint8_t* dst = mask.data_as<std::uint8_t>();
  std::size_t argmax = 0;
  for (std::size_t i = 0; i < vol.size(); ++i) {
    dst[i] = src[i] > threshold ? 1 : 0;
    if (src[i] > src[argmax]) argmax = i;
  }

  // --- Write it back (into the overlay) ------------------------------------------
  // Segmentation derived from the tomogram.
  copick::Segmentation seg = run.new_segmentation(vs.voxel_size(), "proteasome", "auto",
                                                  /*is_multilabel=*/false, "processor",
                                                  /*exist_ok=*/true);
  seg.from_array(mask);

  // A pick at the brightest voxel (index -> z, y, x -> angstrom).
  const std::size_t nx = vol.shape_x(), ny = vol.shape_y();
  const std::size_t z = argmax / (nx * ny), rem = argmax % (nx * ny);
  copick::Point peak;
  peak.location.x = (rem % nx) * vs.voxel_size();
  peak.location.y = (rem / nx) * vs.voxel_size();
  peak.location.z = z * vs.voxel_size();
  copick::Picks picks = run.new_picks("proteasome", "1", "processor", /*exist_ok=*/true);
  std::vector<copick::Point> pts = picks.points();  // keep any existing points
  pts.push_back(peak);
  picks.set_points(pts);
  picks.store();

  // Append an entirely new run.
  root.new_run("TS_002", /*exist_ok=*/true);

  std::cout << "modified project: wrote segmentation 'proteasome/processor/auto', a pick, "
            << "and run TS_002 (" << root.runs().size() << " runs total)\n";
  return 0;
}
