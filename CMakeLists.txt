cmake_minimum_required(VERSION 3.24)  # tensorstore requires >= 3.24
project(copick_cpp VERSION 0.1.0 LANGUAGES CXX)  # x-release-please-version

# --- Options ------------------------------------------------------------------
option(COPICK_BUILD_TESTS "Build the C++ test suite" ON)
option(COPICK_ENABLE_ZARR "Enable tensorstore-backed Zarr I/O (long first build)" OFF)
option(COPICK_ENABLE_MESH "Enable tinygltf GLB mesh I/O" OFF)

# Compiler that verifies the public headers still compile under C++11 (the AreTomo3
# constraint). Defaults to the system `g++`; point it at an older/stricter C++11 compiler
# (or nvcc) for a more faithful check of the AreTomo3 `g++ -std=c++11` build.
set(COPICK_CXX11_GUARD_COMPILER "g++" CACHE STRING "Compiler for the C++11 header guard test")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CopickDependencies.cmake)

# --- Library ------------------------------------------------------------------
# The implementation (src/) compiles at C++20 (reflect-cpp). The PUBLIC headers in
# include/ stay C++11-clean; third-party deps are linked PRIVATE so they never leak
# to consumers (pImpl firewall).
add_library(copick
  src/version.cpp
  src/util/escape.cpp
  src/models/validate.cpp
  src/models/array.cpp
  src/io/json.cpp
)
add_library(copick::copick ALIAS copick)

target_compile_features(copick PRIVATE cxx_std_20)
set_target_properties(copick PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(copick
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(copick PRIVATE COPICK_BUILDING_LIBRARY)
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(copick PUBLIC COPICK_STATIC)
endif()

# PRIVATE deps — hidden behind the firewall.
target_link_libraries(copick PRIVATE reflectcpp)
if(COPICK_ENABLE_MESH)
  target_link_libraries(copick PRIVATE tinygltf)
  target_compile_definitions(copick PRIVATE COPICK_ENABLE_MESH)
endif()
if(COPICK_ENABLE_ZARR)
  target_sources(copick PRIVATE
    src/io/context.cpp
    src/io/kvstore.cpp
    src/io/zarr.cpp
    src/io/ome.cpp
    src/detail/backend.cpp
    src/detail/image.cpp
    src/detail/annotation.cpp
    src/detail/mesh.cpp
  )
  # Link ONLY the drivers we use (zarr v2 + local file kvstore), not tensorstore's
  # `all_drivers` aggregate — that pulls in gcs/grpc/envoy, aws-s3, http/curl, n5,
  # neuroglancer and zarr3, ballooning the binary to ~1 GB and lengthening the build.
  # `driver_zarr` already bundles the blosc/zstd/zlib compressors. When S3 support lands,
  # add `tensorstore::kvstore_s3`.
  target_link_libraries(copick PRIVATE
    tensorstore::tensorstore
    tensorstore::driver_zarr
    tensorstore::kvstore_file
    tensorstore::index_space_dim_expression)  # Dims().HalfOpenInterval() for region I/O
  target_compile_definitions(copick PRIVATE COPICK_ENABLE_ZARR)
endif()

# --- Install ------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS copick
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# --- Examples -----------------------------------------------------------------
option(COPICK_BUILD_EXAMPLES "Build example programs" OFF)
if(COPICK_ENABLE_ZARR AND COPICK_BUILD_EXAMPLES)
  add_executable(write_project examples/write_project.cpp)
  target_link_libraries(write_project PRIVATE copick)
  # Compiled at C++11 to demonstrate that a plain C++11 consumer can use the library.
  target_compile_features(write_project PRIVATE cxx_std_11)

  add_executable(read_project examples/read_project.cpp)
  target_link_libraries(read_project PRIVATE copick)
  target_compile_features(read_project PRIVATE cxx_std_11)

  add_executable(modify_project examples/modify_project.cpp)
  target_link_libraries(modify_project PRIVATE copick)
  target_compile_features(modify_project PRIVATE cxx_std_11)
endif()

# --- Tests --------------------------------------------------------------------
if(COPICK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
