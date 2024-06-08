# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

### Fixed
- Ownership of GError is now properly performed when using GTasks.
- Handle access denied to address family AF_QIPCRTR in lockdown mode.
- Fix verbose argument for enabling debug mode in ssccli.
- Race condition if the sensor is rapidly probed by opening and closing it.

## [0.1.5] - 2024-05-17

### Fixed
- Fixed LICENSE to properly indicate GPLv3.
- Add architecture diagram referenced in reverse engineering notes.
- Fix .so library name to avoid twice 'lib': liblibssc.so

### Added
- Timeout parameter for ssccli.
- Flush stdout after each sensor measurement in ssccli.
- Add support for proximity sensor of xiaomi-davinci (SM7150).

### Changed
- Expanded README with proper description and link to landing page.
- Added build instructions to README.
- Added custom domain for landing page in README.

## [0.1.4] - 2023-08-19

### Fixed
- Link with math library.
- Assert QMI client not NULL during sensor disposing.

### Added
- Define unknown type in QMI Report messages as Report Type.

### Changed
- Update to latest libqmi API changes.
- Search for sensors with a non-zero sample rate such as sensors on Pixel 3A.

## [0.1.3] - 2023-05-20

### Fixed
- Release QMI client before sensor disposing.

## [0.1.2] - 2023-05-20

### Fixed
- Segfault with unknown parameter.
- Segfault during sensor disposing.

## [0.1.1] - 2023-05-01

### Fixed
- Fix format-security warning.

## [0.1.0] - 2023-05-01

Initial release.

<!-- links to diffs between releases -->
[0.1.0]: https://codeberg.org/DylanVanAssche/libssc/compare/01fe59f06aa107a556dd2cdf33e65fd6378eaf32...0.1.0
[0.1.1]: https://codeberg.org/DylanVanAssche/libssc/compare/0.1.0...0.1.1
[0.1.2]: https://codeberg.org/DylanVanAssche/libssc/compare/0.1.1...0.1.2
[0.1.3]: https://codeberg.org/DylanVanAssche/libssc/compare/0.1.2...0.1.3
[0.1.4]: https://codeberg.org/DylanVanAssche/libssc/compare/0.1.3...0.1.4
[0.1.5]: https://codeberg.org/DylanVanAssche/libssc/compare/0.1.4...0.1.5
