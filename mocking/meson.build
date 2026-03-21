pymod = import('python')
python = pymod.find_installation()
python.dependency()

mocking_sources = files(
  'qmi.py',
  'ssc.py',
)

python.install_sources(mocking_sources)

install_data('ssc-server', install_dir: get_option('bindir'))

# Make the SSC Server available during tests
custom_target(
  'ssc-mocking',
  input: mocking_sources,
  output: ['qmi.py', 'ssc.py', 'ssc-server', 'ssc-server-tests'],
  command: ['cp', '@INPUT@', '@OUTDIR@'],
  build_by_default: true
)

subdir('data')
