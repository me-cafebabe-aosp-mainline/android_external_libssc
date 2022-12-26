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

project(
  'ssc-sensor-proxy', 'c',
  version: '0.0.1',
  license: 'GPL3',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=gnu99',
    'warning_level=2',
  ],
  meson_version: '>= 0.53.0',
)

proxy_name = meson.project_name()
proxy_version = meson.project_version()
version_array = proxy_version.split('.')
proxy_version_major = version_array[0].to_int()
proxy_version_minor = version_array[1].to_int()
proxy_version_patch = version_array[2].to_int()

version_conf = configuration_data()
version_conf.set('VERSION', proxy_version)
version_conf.set('PROXY_MAJOR_VERSION', proxy_version_major)
version_conf.set('PROXY_MINOR_VERSION', proxy_version_minor)
version_conf.set('PROXY_PATCH_VERSION', proxy_version_patch)

proxy_includedir = get_option('includedir')
proxy_glib_include_subdir = proxy_name + '-glib'
proxy_glib_pkgincludedir = proxy_includedir / proxy_glib_include_subdir

cc = meson.get_compiler('c')
cc_flags = cc.get_supported_arguments([
  '-Wno-unused-parameter',
  '-Wno-cast-function-type',
  '-Wno-packed',
])
add_project_arguments(cc_flags, language: 'c')

glib_version = '2.56'
libqmi_version = '1.33.4'

c_flags = [
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_' + glib_version.underscorify(),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_' + glib_version.underscorify(),
  '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
]

glib_dep = dependency('glib-2.0', version: '>= ' + glib_version)
gio_unix_dep = dependency('gio-unix-2.0')
libqmi_dep = dependency('qmi-glib', version: '>=' + libqmi_version)
protobufc_dep = dependency('libprotobuf-c')
protoc_tool = find_program('protoc', 'protoc-c')

deps = [
  glib_dep,
  dependency('gio-2.0'),
  dependency('gobject-2.0'),
  libqmi_dep,
  protobufc_dep
]

glib_deps = declare_dependency(
  dependencies: deps,
  compile_args: c_flags
)

top_inc = include_directories('.')
sources = files()
headers = files()

subdir('data')
subdir('src')

