/*
 * libssc: Library to expose Qualcomm Sensor Core sensors
 * Copyright (C) 2022-2023 Dylan Van Assche
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

#ifndef _LIBSSC_SENSOR_PROXIMITY_H_
#define _LIBSSC_SENSOR_PROXIMITY_H_

#include <glib.h>
#include <stdbool.h>
#include "libssc-client.h"
#include "libssc-common.h"
#include "ssc-sensor-proximity.pb-c.h"

#define SSC_SENSOR_TYPE_PROXIMITY		"proximity"
#define SSC_MSG_RESPONSE_PROXIMITY		769
#define SSC_SENSOR_PROXIMITY_NEAR		1
#define SSC_SENSOR_PROXIMITY_FAR		0

void
ssc_sensor_proximity_open (SSCClient *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);

gboolean
ssc_sensor_proximity_open_finish (SSCClient *self, GAsyncResult *res, GError **error);

gboolean
ssc_sensor_proximity_open_sync (SSCClient *self, gchar *data_type, GError **error);

void
ssc_sensor_proximity_close (SSCClient *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);

gboolean
ssc_sensor_proximity_close_finish (SSCClient *self, GAsyncResult *res, GError **error);

gboolean
ssc_sensor_proximity_close_sync (SSCClient *self, gchar *data_type, GError **error);

#endif /* _LIBSSC_SENSOR_PROXIMITY_H_ */
