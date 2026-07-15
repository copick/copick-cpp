# Portable build-environment helper — automates the README "conda toolchain" steps with no
# machine-specific paths. SOURCE it (don't execute), then build with the presets:
#
#   source build_env.sh
#   cmake --preset full && cmake --build build-full --target copick_tests
#
# It creates (once) and activates a conda-forge env with a C++20 compiler + nasm + cmake +
# ninja, and exports CC/CXX. Override the env name with COPICK_CONDA_ENV.

_copick_env="${COPICK_CONDA_ENV:-copick-cpp}"

if ! command -v conda >/dev/null 2>&1; then
  echo "build_env.sh: 'conda' not found. Install Miniforge/conda, or set up a C++20 compiler" >&2
  echo "              (GCC >= 11 / Clang >= 14) + nasm + cmake + ninja yourself (see README)." >&2
  return 1 2>/dev/null || exit 1
fi

# Create the toolchain env once (mirrors the README's `conda create ...`).
if ! conda env list | awk '{print $1}' | grep -qx "${_copick_env}"; then
  echo "build_env.sh: creating conda env '${_copick_env}' (gxx_linux-64 nasm cmake ninja)..."
  conda create -y -n "${_copick_env}" -c conda-forge gxx_linux-64 gcc_linux-64 nasm cmake ninja \
    || { echo "build_env.sh: env creation failed." >&2; return 1 2>/dev/null || exit 1; }
fi

conda activate "${_copick_env}" \
  || { echo "build_env.sh: could not activate '${_copick_env}' (is conda initialised in this shell?)." >&2
       return 1 2>/dev/null || exit 1; }

# The conda compiler package exports CC/CXX on activation; fall back to plain names.
: "${CC:=$(command -v gcc)}"
: "${CXX:=$(command -v g++)}"
export CC CXX

echo "build_env.sh: env='${_copick_env}'  CXX=${CXX}  nasm=$(command -v nasm || echo MISSING)"
unset _copick_env
