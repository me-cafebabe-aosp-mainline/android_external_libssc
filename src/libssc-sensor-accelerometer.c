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

#include "libssc-sensor-accelerometer.h"

enum {
	SIGNAL_MEASUREMENT,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

typedef struct _SSCSensorAccelerometerPrivate {
	guint report_id;
	GFile *file;
} SSCSensorAccelerometerPrivate;

G_DEFINE_TYPE_WITH_CODE (SSCSensorAccelerometer, ssc_sensor_accelerometer, SSC_TYPE_SENSOR,
			 G_ADD_PRIVATE (SSCSensorAccelerometer))

/*****************************************************************************/

static void
accelerometer_close_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->close_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_accelerometer_close_finish (SSCSensorAccelerometer *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_accelerometer_close (SSCSensorAccelerometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->close &&
		  SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->close_finish);

	task = g_task_new (self, cancellable, callback, user_data);

	SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->close (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)accelerometer_close_ready, task);
}

/*****************************************************************************/

static void
accelerometer_open_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->open_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_accelerometer_open_finish (SSCSensorAccelerometer *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_accelerometer_open (SSCSensorAccelerometer *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->open &&
		  SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->open_finish);

	task = g_task_new (self, cancellable, callback, user_data);

	SSC_SENSOR_CLASS (ssc_sensor_accelerometer_parent_class)->open (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)accelerometer_open_ready, task);
}

/*****************************************************************************/

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SscAccelerometerResponse *msg = NULL;
	SSCSensorAccelerometer *sensor = SSC_SENSOR_ACCELEROMETER (user_data);
	guint64 sensor_uid_low;
	guint64 sensor_uid_high;
	gfloat x;
	gfloat y;
	gfloat z;

	g_object_get (sensor,
		      SSC_SENSOR_UID_HIGH, &sensor_uid_high,
		      SSC_SENSOR_UID_LOW, &sensor_uid_low,
		      NULL);

	if (sensor_uid_high == uid_high && sensor_uid_low == uid_low && msg_id == SSC_MSG_REPORT_MEASUREMENT_ACCELEROMETER) {
		msg = ssc_accelerometer_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

		if (msg->n_acceleration >= 3) {
			x = msg->acceleration[0];
			y = msg->acceleration[1];
			z = msg->acceleration[2];

			g_debug ("Accelerometer sensor measurement: X=%f Y=%f Z=%f m/s²", x, y, z);
			g_signal_emit (sensor, signals[SIGNAL_MEASUREMENT], 0, x, y, z); 
		}

		ssc_accelerometer_response__free_unpacked (msg, NULL);
	}
}

/*****************************************************************************/

static void
ssc_sensor_accelerometer_class_init (SSCSensorAccelerometerClass *klass)
{
	signals[SIGNAL_MEASUREMENT] = g_signal_new ("measurement",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE,
		3, G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT);
}

static void
ssc_sensor_accelerometer_init (SSCSensorAccelerometer *self)
{
}

SSCSensorAccelerometer *
ssc_sensor_accelerometer_new_finish (GAsyncResult *result, GError **error)
{
	SSCSensorAccelerometerPrivate *priv = NULL;
	SSCClient *client = NULL;
	GObject *sensor;
	GObject *source;

	source = g_async_result_get_source_object (result);
	sensor = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);

	if (!sensor) {
		g_object_unref (source);
		return NULL;
	}

	priv = ssc_sensor_accelerometer_get_instance_private (SSC_SENSOR_ACCELEROMETER (sensor));

	/* Start listening for reports */
	g_object_get (SSC_SENSOR (sensor),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);
	priv->report_id = g_signal_connect (client,
			"report",
			G_CALLBACK (report_received),
			sensor);

	g_object_unref (source);
	return SSC_SENSOR_ACCELEROMETER (sensor);
}

void
ssc_sensor_accelerometer_new (GFile *file, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_async_initable_new_async (
			SSC_TYPE_SENSOR_ACCELEROMETER,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
			SSC_SENSOR_DATA_TYPE, "accel",
			SSC_CLIENT_FILE_PATH, file,
			NULL);
}
