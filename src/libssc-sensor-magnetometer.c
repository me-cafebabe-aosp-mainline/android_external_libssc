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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "libssc-sensor-magnetometer.h"

enum {
	SIGNAL_MEASUREMENT,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

typedef struct _SSCSensorMagnetometerPrivate {
	guint report_id;
	GFile *file;
} SSCSensorMagnetometerPrivate;

G_DEFINE_TYPE_WITH_CODE (SSCSensorMagnetometer, ssc_sensor_magnetometer, SSC_TYPE_SENSOR,
			 G_ADD_PRIVATE (SSCSensorMagnetometer))

/*****************************************************************************/

static void
magnetometer_close_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_magnetometer_parent_class)->close_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_magnetometer_close_finish (SSCSensorMagnetometer *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_magnetometer_close (SSCSensorMagnetometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_magnetometer_parent_class)->close &&
		  SSC_SENSOR_CLASS (ssc_sensor_magnetometer_parent_class)->close_finish);

	task = g_task_new (self, cancellable, callback, user_data);

	SSC_SENSOR_CLASS (ssc_sensor_magnetometer_parent_class)->close (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)magnetometer_close_ready, task);
}

/*****************************************************************************/

static void
magnetometer_open_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_magnetometer_parent_class)->open_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_magnetometer_open_finish (SSCSensorMagnetometer *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_magnetometer_open (SSCSensorMagnetometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_magnetometer_parent_class)->open &&
		  SSC_SENSOR_CLASS (ssc_sensor_magnetometer_parent_class)->open_finish);

	task = g_task_new (self, cancellable, callback, user_data);

	SSC_SENSOR_CLASS (ssc_sensor_magnetometer_parent_class)->open (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)magnetometer_open_ready, task);
}

/*****************************************************************************/

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SscMagnetometerResponse *msg = NULL;
	SSCSensorMagnetometer *sensor = SSC_SENSOR_MAGNETOMETER (user_data);
	guint64 sensor_uid_low;
	guint64 sensor_uid_high;
	gfloat x;
	gfloat y;
	gfloat z;

	g_object_get (sensor,
		      SSC_SENSOR_UID_HIGH, &sensor_uid_high,
		      SSC_SENSOR_UID_LOW, &sensor_uid_low,
		      NULL);

	if (sensor_uid_high == uid_high && sensor_uid_low == uid_low && msg_id == SSC_MSG_REPORT_MEASUREMENT_MAGNETOMETER) {
		msg = ssc_magnetometer_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

		if (msg->n_magnetic_field >= 3) {
			x = msg->magnetic_field[0];
			y = msg->magnetic_field[1];
			z = msg->magnetic_field[2];

			g_debug ("Magnetometer sensor measurement: X=%f Y=%f Z=%f μT", x, y, z);
			g_signal_emit (sensor, signals[SIGNAL_MEASUREMENT], 0, x, y, z); 
		}

		ssc_magnetometer_response__free_unpacked (msg, NULL);
	}
}

/*****************************************************************************/

static void
ssc_sensor_magnetometer_class_init (SSCSensorMagnetometerClass *klass)
{
	signals[SIGNAL_MEASUREMENT] = g_signal_new ("measurement",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE,
		3, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT);
}

static void
ssc_sensor_magnetometer_init (SSCSensorMagnetometer *self)
{
}

SSCSensorMagnetometer *
ssc_sensor_magnetometer_new_finish (GAsyncResult *result, GError **error)
{
	SSCSensorMagnetometerPrivate *priv = NULL;
	SSCClient *client = NULL;
	GObject *sensor;
	GObject *source;

	source = g_async_result_get_source_object (result);
	sensor = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);

	if (!sensor) {
		g_object_unref (source);
		return NULL;
	}

	priv = ssc_sensor_magnetometer_get_instance_private (SSC_SENSOR_MAGNETOMETER (sensor));

	/* Start listening for reports */
	g_object_get (SSC_SENSOR (sensor),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);
	priv->report_id = g_signal_connect (client,
			"report",
			G_CALLBACK (report_received),
			sensor);

	g_object_unref (source);
	return SSC_SENSOR_MAGNETOMETER (sensor);
}

void
ssc_sensor_magnetometer_new (GFile *file, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_async_initable_new_async (
			SSC_TYPE_SENSOR_MAGNETOMETER,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
			SSC_SENSOR_DATA_TYPE, "mag",
			SSC_CLIENT_FILE_PATH, file,
			NULL);
}
