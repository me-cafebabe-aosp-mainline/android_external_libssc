# SPDX-License-Identifier: GPL-3.0-or-later
#
# ssc-sensor-proxy: a drop-in replacement for iio-sensor-proxy for SSC support
# Copyright (C) 2022 Dylan Van Assche
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

proxy_glib_inc = include_directories('.')

version_header = configure_file(
  input: 'proxy-version.h.in',
  output: '@BASENAME@',
  configuration: version_conf,
  install_dir: proxy_glib_pkgincludedir,
)

headers = files(
  'ssc-sensor-proxy.h'
)
install_headers(
  headers, 
  install_dir: proxy_glib_pkgincludedir
)

sources = files(
  'ssc-sensor-proxy.c',
)

ssc_sensor_proxy = executable(
  'ssc-sensor-proxy',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  install: true,
)
