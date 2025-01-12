# Release instructions

1. Update [CHANGELOG.md](CHANGELOG.md) with the latest changes
2. Update version tag in `meson.build`.
3. Update SSC Server version in `mocking/ssc-server`.
4. Tag an annotated release with semantic versioning: `git tag -s -m "libssc X.Y.Z" vX.Y.Z`
