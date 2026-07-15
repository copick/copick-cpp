// Forward declarations of the copick handle classes and their private implementations.
//
// PUBLIC HEADER — must compile under -std=c++11.
#ifndef COPICK_FWD_H
#define COPICK_FWD_H

namespace copick {

class Root;
class Run;
class VoxelSpacing;
class Tomogram;
class Features;
class Object;
class Picks;
class Mesh;
class Segmentation;

namespace detail {
class RootImpl;
class RunImpl;
class VoxelSpacingImpl;
class TomogramImpl;
class FeaturesImpl;
class ObjectImpl;
class PicksImpl;
class MeshImpl;
class SegmentationImpl;
}  // namespace detail

}  // namespace copick

#endif  // COPICK_FWD_H
