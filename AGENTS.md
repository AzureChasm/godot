# Repository Guidelines

## Project Structure & Module Organization

This is the Astray Godot fork. Keep custom terrain and database features in `modules/` where practical. Game scenes and UI belong in the separate game repository. Run commands from this root.

- `core/` contains foundational types; `scene/` implements scene nodes and resources.
- `servers/`, `drivers/`, and `platform/` contain engine services, backend integrations, and platform support.
- `editor/` contains editor code and icons; `modules/` contains optional engine features.
- `tests/` contains engine tests; module-specific tests also live under `modules/`.
- `doc/classes/` contains API reference XML; `misc/` contains development tools and distribution assets.
- `thirdparty/` holds bundled dependencies; `bin/` holds build output.

## Build, Test, and Development Commands

Use Python 3, SCons, a supported C++ toolchain, and platform dependencies. Linux commands:

- `scons platform=linuxbsd target=editor -j4`: build the editor.
- `./bin/godot.linuxbsd.editor.x86_64 --editor`: launch the built editor.
- `scons platform=linuxbsd target=editor dev_build=yes tests=yes -j4`: build a development editor with tests.
- `./bin/godot.linuxbsd.editor.dev.x86_64 --headless --test`: run engine tests.
- `pre-commit install`: enable repository hooks.
- `pre-commit run --all-files`: run formatting, linting, and validation hooks.

Binary names depend on build options and architecture.

## Coding Style & Naming Conventions

Follow `.editorconfig` and nearby code. Use tabs with width four for C++, four spaces for Python/SCons, and two spaces for YAML. Preserve UTF-8, LF endings, and final newlines. Use `snake_case` filenames and functions, and `PascalCase` C++ types. Hooks configure clang-format, Ruff, mypy, and codespell. Avoid unrelated formatting changes or edits to bundled dependencies.

## Testing Guidelines

C++ tests use doctest. Add `test_*.cpp` files in the matching subsystem and descriptive cases such as `TEST_CASE("[XMLParser] Premature endings")`. Include regression tests with fixes and cover success and failure paths for features. No numeric coverage threshold is specified.

## Commit & Pull Request Guidelines

Use concise, capitalized, imperative commit titles, preferably under 72 characters, such as `Fix scroll hints in the Quick Open dialog`. Keep commits buildable and PRs focused; rebase instead of introducing merge commits. Target `master`, explain the problem and validation, and link issues with `Fixes #1234` in the PR description. Disclose AI assistance as required by the PR template. Update reference XML for scripting API changes. Consult `CONTRIBUTING.md` before submitting.

## Fork Maintenance

`misc/fork-maintenance/` contains the Friday 05:00 Europe/Berlin updater. It fast-forwards clean `master` to upstream, pushes only `master` to `origin`, and merges into local `develop`. Automatic pushes of clean `master` are authorized; never push `develop` without explicit authorization. Rebuild and smoke-test after upstream changes.
