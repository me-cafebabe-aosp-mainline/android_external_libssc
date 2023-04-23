# SPDX-License-Identifier: GPL-3.0-or-later
#
# libssc: Library to expose Qualcomm Sensor Core sensors
# Copyright (C) 2022-2023 Dylan Van Assche
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#  
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

libssc_glib_inc = include_directories('.')

version_header = configure_file(
  input: 'libssc-version.h.in',
  output: '@BASENAME@',
  configuration: version_conf,
  install_dir: libssc_glib_pkgincludedir,
)

headers += files(
  'libssc-common.h',
  'libssc-client.h',
  'libssc-sensor.h',
  'libssc-sensor-proximity.h',
  'libssc-sensor-light.h',
  'libssc-sensor-accelerometer.h',
  'libssc-sensor-magnetometer.h',
)

cli_headers = files(
  'libssc-cli.h',
)

install_headers(
  headers + cli_headers, 
  install_dir: libssc_glib_pkgincludedir
)

sources += files(
  'libssc-common.c',
  'libssc-client.c',
  'libssc-sensor.c',
  'libssc-sensor-proximity.c',
  'libssc-sensor-light.c',
  'libssc-sensor-accelerometer.c',
  'libssc-sensor-magnetometer.c',
)

cli_sources = files(
  'libssc-cli.c',
)

libssc = library(
  'libssc',
  sources: sources,
  include_directories: includes,
  dependencies: deps,
  install: true,
  soversion: '0',
)

ssccli = executable(
  'ssccli',
  sources: cli_sources,
  include_directories: includes,
  dependencies: deps,
  link_with: libssc,
  install: true,
)

ns = 'SSC'

pkg.generate(
  libraries: libssc,
  version: libssc_version,
  name: 'libssc-glib',
  description: 'Library to access sensors of Qualcomm Sensor Core',
  subdirs: libssc_glib_include_subdir,
  requires: ['glib-2.0', 'gobject-2.0', 'gio-2.0', 'qmi-glib'],
)
