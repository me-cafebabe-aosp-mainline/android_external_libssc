test(
  'tests',
  executable('tests', 'tests.c', link_with: libssc, dependencies: deps),
  env: [
    'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
    'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  ],
  protocol: 'tap',
)
