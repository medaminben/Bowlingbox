# Contributing to Bowlingbox

Thanks for helping out. Bug reports, ideas and pull requests are all welcome.

## Reporting bugs and asking for features

Open an [issue](https://github.com/medaminben/Bowlingbox/issues/new/choose) and pick the bug report
or feature request form. For a bug, say which app (BowlingGL, BowlingConsole, BowlingDisplay or a
library), your OS, compiler, CMake and wxWidgets versions, and the steps that show it.

Do not report security problems in a public issue. [SECURITY.md](SECURITY.md) explains how to report
them privately.

## Making a change

1. Fork the repository and create a branch from `main`.
2. Build and run the tests. The [manual](docs/MANUAL.md) lists the packages you need.

   ```bash
   cmake -S . -B build
   cmake --build build -j
   ctest --test-dir build --output-on-failure
   ```

3. Add or update tests for what you change. After changes to Alley, Render or BowlingGL, also go
   through the manual checklist in [applications/bowlingGame/README.md](applications/bowlingGame/README.md).
4. Update the documentation (`README.md`, `docs/MANUAL.md`, `docs/ARCHITECTURE.md`) when behaviour,
   build options or the public API change.
5. Open a pull request against `main` and fill in the template.

The [developer guide](docs/MANUAL.md#8-developer-guide) covers coverage, clang-tidy, adding a library,
and the code conventions: C++20, warnings as errors, public API under `include/Bowlingbox/<Lib>/`,
dependencies on interfaces, and lowercase test names that start with `test_`.

## Commits

Use short messages in the form `type(scope): what changed`, for example
`fix(display): game score box fits 300`. Common types are `feat`, `fix`, `refactor`, `docs`, `build`,
`ci` and `test`.

## Pull requests and versions

CI builds and tests every pull request on Ubuntu 24.04. A pull request is merged once CI is green and
a maintainer has reviewed it.

Each merged pull request is released with its own version. Add one label to say how big the change is:
`semver:major` (breaking change to the public library API), `semver:minor` (new feature),
`semver:patch` (fix or refactor, the default) or `semver:none` (docs or CI only, no release). The
[versioning section](docs/MANUAL.md#versioning) of the manual has the details.

## Code of conduct

Everyone taking part in this project follows the [Code of Conduct](CODE_OF_CONDUCT.md).

## License

By contributing you agree that your contributions are licensed under the [MIT License](LICENSE).
