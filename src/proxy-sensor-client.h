/*
 * ssc-sensor-proxy: a drop-in replacement for iio-sensor-proxy for SSC support
 * Copyright (C) 2022 Dylan Van Assche
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _PROXY_SENSOR_CLIENT_H_
#define _PROXY_SENSOR_CLIENT_H_

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <stdbool.h>

#include "ssc-shared.pb-c.h"
#include "ssc-suid-sensor.pb-c.h"

gboolean
sensor_client_init(GFile *file);

#endif /* _PROXY_SENSOR_CLIENT_H_ */
