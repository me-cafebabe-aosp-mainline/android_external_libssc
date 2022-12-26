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

msgs = [
  'shared',
  'suid-sensor',
]

foreach m: msgs
  name = 'ssc-' + m
  protobuf_gen = custom_target (
    name,
    input: name + '.proto',
    output: [name + '.pb-c.c', name + '.pb-c.h'],
    command: [protoc_tool, '--proto_path=@CURRENT_SOURCE_DIR@', '--c_out=@OUTDIR@', '@INPUT@'],
    install: true,
    install_dir: [false, proxy_glib_pkgincludedir]
  )
endforeach
