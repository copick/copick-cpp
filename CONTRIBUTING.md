# Contributing to copick-cpp

## Development setup

```bash
# One-time: install pre-commit hooks (formatting, lint, commit-message checks)
pip install pre-commit
pre-commit install --install-hooks
pre-commit install --hook-type commit-msg

# Build + test (the `full` build needs nasm on PATH: conda install -c conda-forge nasm)
cmake --preset dev  && cmake --build build      && ctest --test-dir build
cmake --preset full && cmake --build build-full && ctest --test-dir build-full
```

The presets use your default compiler. If it isn't a C++20 compiler (GCC ≥ 11 / Clang ≥ 14),
point at one with a local, gitignored `CMakeUserPresets.json`:

```json
{
  "version": 3,
  "configurePresets": [
    { "name": "dev-local", "inherits": "dev",
      "cacheVariables": { "CMAKE_CXX_COMPILER": "/path/to/g++", "CMAKE_C_COMPILER": "/path/to/gcc" } },
    { "name": "full-local", "inherits": ["dev-local", "full"] }
  ]
}
```

then `cmake --preset full-local`. See the README for the conda-toolchain alternative.

## Code style & linting

- **clang-format** (`.clang-format`, Google style, 100 cols) — enforced in CI and by
  pre-commit. Run `pre-commit run --all-files` (or `clang-format -i <files>`) to format.
- **clang-tidy** (`.clang-tidy`) — runs in CI (informational). `modernize-*` /
  `cppcoreguidelines-*` are disabled on purpose: the public headers are seen through the
  C++20 implementation TUs, and those checks would push C++14/17/20 constructs into the
  **C++11** public API.
- The **C++11 header guard** CTest compiles every public header under `-std=c++11`; keep
  `include/copick/` free of C++14+ syntax and third-party includes.

## Commit messages — Conventional Commits

This repo uses [Conventional Commits](https://www.conventionalcommits.org/) so that
[release-please](https://github.com/googleapis/release-please) can automate versioning and
changelogs. The `commitizen` commit-msg hook checks the format. Examples:

```
feat: add S3 kvstore backend
fix: correct voxel-spacing directory rounding
docs: document the overlay merge
feat!: rename Tomogram::numpy region argument   # ! = breaking change -> major bump
```

`feat:` → minor bump, `fix:` → patch bump, `feat!:`/`BREAKING CHANGE:` → major. Other
types (`docs`, `test`, `refactor`, `build`, `ci`, `chore`) don't trigger a release.

## Releases

Merging to `main` updates a release PR (via `release-please.yml`). Merging that PR tags a
release, bumps `version.txt` + `include/copick/version.h`, and updates `CHANGELOG.md`.
Releases are **source-only** (GitHub attaches the source archives automatically) — no
prebuilt binary is published, since a compiled `libcopick` is only usable against the exact
tensorstore/reflect-cpp versions it was linked with. Build from source per the README.

## Cross-implementation parity

Changes to storage/serialization should keep byte-compatibility with Python copick. See
`tests/scripts/` (`verify_parity.py`, `write_with_python.py`) and the `SampleData` tests
(`COPICK_SAMPLE_DIR` + `fetch_sample_data.py`).
