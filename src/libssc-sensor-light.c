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

#include "libssc-sensor-light.h"

enum {
	SIGNAL_MEASUREMENT,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

typedef struct _SSCSensorLightPrivate {
	guint report_id;
	GFile *file;
} SSCSensorLightPrivate;

G_DEFINE_TYPE_WITH_CODE (SSCSensorLight, ssc_sensor_light, SSC_TYPE_SENSOR,
			 G_ADD_PRIVATE (SSCSensorLight))

/*****************************************************************************/

static void
light_close_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_light_parent_class)->close_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_light_close_finish (SSCSensorLight *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_light_close (SSCSensorLight *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_light_parent_class)->close &&
		  SSC_SENSOR_CLASS (ssc_sensor_light_parent_class)->close_finish);

	task = g_task_new (self, cancellable, callback, user_data);

	SSC_SENSOR_CLASS (ssc_sensor_light_parent_class)->close (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)light_close_ready, task);
}

/*****************************************************************************/

static void
light_open_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_light_parent_class)->open_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_light_open_finish (SSCSensorLight *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_light_open (SSCSensorLight *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_light_parent_class)->open &&
		  SSC_SENSOR_CLASS (ssc_sensor_light_parent_class)->open_finish);

	task = g_task_new (self, cancellable, callback, user_data);

	SSC_SENSOR_CLASS (ssc_sensor_light_parent_class)->open (SSC_SENSOR (self), cancellable, (GAsyncReadyCallback)light_open_ready, task);
}

/*****************************************************************************/

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SscLightResponse *msg = NULL;
	SSCSensorLight *sensor = SSC_SENSOR_LIGHT (user_data);
	guint64 sensor_uid_low;
	guint64 sensor_uid_high;
	gfloat intensity;

	g_object_get (sensor,
		      SSC_SENSOR_UID_HIGH, &sensor_uid_high,
		      SSC_SENSOR_UID_LOW, &sensor_uid_low,
		      NULL);

	if (sensor_uid_high == uid_high && sensor_uid_low == uid_low && msg_id == SSC_MSG_REPORT_MEASUREMENT_LIGHT) {
		msg = ssc_light_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

		/* Only report intensity in Lux, raw sensor values are ignored */
		if (msg->n_intensity >= 1) {
			intensity = msg->intensity[0];
			g_debug ("Light sensor measurement: %f Lux", intensity);
			g_signal_emit (sensor, signals[SIGNAL_MEASUREMENT], 0, intensity); 
		}

		ssc_light_response__free_unpacked (msg, NULL);
	}
}

/*****************************************************************************/

static void
ssc_sensor_light_class_init (SSCSensorLightClass *klass)
{
	signals[SIGNAL_MEASUREMENT] = g_signal_new ("measurement",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE,
		1, G_TYPE_FLOAT);
}

static void
ssc_sensor_light_init (SSCSensorLight *self)
{
}

SSCSensorLight *
ssc_sensor_light_new_finish (GAsyncResult *result, GError **error)
{
	SSCSensorLightPrivate *priv = NULL;
	SSCClient *client = NULL;
	GObject *sensor;
	GObject *source;

	source = g_async_result_get_source_object (result);
	sensor = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);

	if (!sensor) {
		g_object_unref (source);
		return NULL;
	}

	priv = ssc_sensor_light_get_instance_private (SSC_SENSOR_LIGHT (sensor));

	/* Start listening for reports */
	g_object_get (SSC_SENSOR (sensor),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);
	priv->report_id = g_signal_connect (client,
			"report",
			G_CALLBACK (report_received),
			sensor);

	g_object_unref (source);
	return SSC_SENSOR_LIGHT (sensor);
}

void
ssc_sensor_light_new (GFile *file, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_async_initable_new_async (
			SSC_TYPE_SENSOR_LIGHT,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
			SSC_SENSOR_DATA_TYPE, "ambient_light",
			SSC_CLIENT_FILE_PATH, file,
			NULL);
}
