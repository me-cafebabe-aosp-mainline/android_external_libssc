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

#ifndef _LIBSSC_SENSOR_SUID_H_
#define _LIBSSC_SENSOR_SUID_H_

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gio/gio.h>
#include <libqmi-glib.h>
#include <stdbool.h>
#include "libssc-client.h"
#include "ssc-common.pb-c.h"
#include "ssc-sensor-suid.pb-c.h"

#define SUID_SENSOR_TYPE_UID_LOOKUP                "suid"
#define SUID_SENSOR_TYPE_PROXIMITY                 "proximity"
#define SUID_SENSOR_TYPE_ACCELEROMETER             "accel"
#define SUID_SENSOR_TYPE_ACCELEROMETER_CALLIBRATED "accel_cal"
#define SUID_SENSOR_TYPE_LIGHT                     "ambient_light"
#define SUID_SENSOR_TYPE_GYROSCOPE                 "gyro"
#define SUID_SENSOR_TYPE_GYROSCOPE_CALLIBRATED     "gyro_cal"
#define SUID_SENSOR_TYPE_GYROSCOPE_MATRIX          "gyro_rot_matrix"
#define SUID_SENSOR_TYPE_GRAVITY                   "gravity"
#define SUID_SENSOR_TYPE_MAGNETOMETER              "mag"
#define SUID_SENSOR_TYPE_MAGNETOMETER_CALLIBRATED  "mag_cal"
#define SUID_SENSOR_UID_LOW			   0xABABABABABABABABUL
#define SUID_SENSOR_UID_HIGH			   0xABABABABABABABABUL
#define SUID_SENSOR_MSG_EVENT			   768
#define SUID_SENSOR_MSG_REQUEST			   512

#define SSC_PROCESSOR_APSS			   1
#define SSC_SUSPEND_MODE_WAKEUP			   0

void
ssc_sensor_suid_lookup (SSCClient *self, gchar *data_type, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);

SSCSensor *
ssc_sensor_suid_lookup_finish (SSCClient *self, GAsyncResult *res, GError **error);

SSCSensor *
ssc_sensor_suid_lookup_sync (SSCClient *self, gchar *data_type, GError **error);

#endif /* _LIBSSC_SENSOR_SUID_H_ */
