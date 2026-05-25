pymod = import('python')
python = pymod.find_installation()
python.dependency()

mocking_sources = files(
  'qmi.py',
  'ssc.py',
)
python.install_sources(mocking_sources)

ssc_server_conf = configuration_data()
ssc_server_conf.set('VERSION', libssc_version)
ssc_server_conf.set('LIBSSC_MAJOR_VERSION', libssc_version_major)
ssc_server_conf.set('LIBSSC_MINOR_VERSION', libssc_version_minor)
ssc_server_conf.set('LIBSSC_PATCH_VERSION', libssc_version_patch)
ssc_server_conf.set('PROTOBUF_PYTHON_BUILD_PATH', join_paths(meson.global_build_root(), 'data'))

ssc_server = configure_file(
  input: 'ssc-server.in',
  output: '@BASENAME@',
  configuration: ssc_server_conf,
  install_dir: installed_tests_execdir,
)

# Make the SSC Server available during tests
custom_target(
  'ssc-mocking',
  input: mocking_sources,
  output: ['qmi.py', 'ssc.py', 'ssc-server', 'ssc-server-tests'],
  command: ['cp', '@INPUT@', '@OUTDIR@'],
  build_by_default: true
)

subdir('data')
