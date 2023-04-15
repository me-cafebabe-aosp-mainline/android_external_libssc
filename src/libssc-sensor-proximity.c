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

#include "libssc-sensor-proximity.h"

enum {
	SIGNAL_MEASUREMENT_PROXIMITY,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

typedef struct _SSCSensorProximity {
	GObject parent;
} SSCSensorProximity;

typedef struct _SSCSensorProximityPrivate {
	guint report_id;
} SSCSensorProximityPrivate;

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SSCSensorProximity, ssc_sensor_proximity, SSC_TYPE_SENSOR_PROXIMITY,
			 G_ADD_PRIVATE (SSCSensorProximity)
			 G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init));

/*****************************************************************************/

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SscProximityResponse *msg = NULL;
	SSCSensorProximity *sensor = SSC_SENSOR_PROXIMITY (user_data);
	guint64 sensor_uid_low;
	guint64 sensor_uid_high;
	gboolean near = false;

	g_debug ("REPORT RECEIVED");

	g_object_get (sensor,
		      SSC_SENSOR_UID_HIGH, &sensor_uid_high,
		      SSC_SENSOR_UID_LOW, &sensor_uid_low,
		      NULL);

	g_debug ("GOT UID: %016lX %016lX", sensor_uid_high, sensor_uid_low);

	if (sensor_uid_high == uid_high && sensor_uid_low == uid_low && msg_id == SSC_MSG_REPORT_MEASUREMENT_PROXIMITY) {
		msg = ssc_proximity_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

		switch (msg->near) {
			case SSC_SENSOR_PROXIMITY_NEAR:
				near = true;
				break;
			case SSC_SENSOR_PROXIMITY_FAR:
				near = false;
				break;
			default:
				g_assert_not_reached ();
		}

		g_debug ("Proximity sensor measurement: %s", near ? "near" : "far");
		g_signal_emit (sensor, signals[SIGNAL_MEASUREMENT_PROXIMITY], 0, near); 

		ssc_proximity_response__free_unpacked (msg, NULL);
	}
}

/*****************************************************************************/

void
ssc_sensor_proximity_close (SSCSensorProximity *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	return ssc_sensor_close (SSC_SENSOR (self), cancellable, callback, user_data);
}

gboolean
ssc_sensor_proximity_close_finish (SSCSensorProximity *self, GAsyncResult *result, GError **error)
{
	return ssc_sensor_close_finish (SSC_SENSOR (self), result, error);
}

/*****************************************************************************/

void
ssc_sensor_proximity_open (SSCSensorProximity *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	return ssc_sensor_open (SSC_SENSOR (self), cancellable, callback, user_data);
}

gboolean
ssc_sensor_proximity_open_finish (SSCSensorProximity *self, GAsyncResult *result, GError **error)
{
	return ssc_sensor_open_finish (SSC_SENSOR (self), result, error);
}

/*****************************************************************************/

static void
sensor_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;
	SSCSensorProximity *self = NULL;

	self = g_task_get_source_object (task);

	if (!ssc_sensor_new_finish (result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);

	g_debug ("SENSOR READY");
}

static void
initable_init_async (GAsyncInitable *initable, int io_priority, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensorProximity *self = NULL;
	g_autoptr (GFile) file = NULL;
	gchar *data_type = NULL;

	g_debug ("*** initable_init_async ***");
	
	self = SSC_SENSOR_PROXIMITY (initable);
	task = g_task_new (self, cancellable, callback, user_data);

	g_object_get (self,
		      SSC_CLIENT_FILE_PATH, &file,
		      SSC_SENSOR_DATA_TYPE, &data_type,
		      NULL);
	ssc_sensor_new (file,
			data_type,	
			NULL,
			(GAsyncReadyCallback)sensor_ready,
			task);
}

static gboolean
initable_init_finish (GAsyncInitable *initable, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
	iface->init_async = initable_init_async;
	iface->init_finish = initable_init_finish;

	g_debug ("*** async_initable_iface_init ***");
}

static void
ssc_sensor_proximity_class_init (SSCSensorProximityClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	g_debug ("*** ssc_sensor_proximity_class_init ***");

	/* Signals */
	signals[SIGNAL_MEASUREMENT_PROXIMITY] = g_signal_new ("measurement-proximity",
			G_OBJECT_CLASS_TYPE (object_class),
			G_SIGNAL_RUN_FIRST,
			0, NULL, NULL, NULL,
			G_TYPE_NONE,
			1, G_TYPE_BOOLEAN);
}

static void
ssc_sensor_proximity_init (SSCSensorProximity *self)
{
	SSCClient *client = NULL;
	SSCSensorProximityPrivate *priv = NULL;

	priv = ssc_sensor_proximity_get_instance_private (self);
	g_object_get (SSC_SENSOR (self),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);

	priv->report_id = g_signal_connect (client,
					    "report",
					    G_CALLBACK (report_received),
					    self);
}

SSCSensorProximity *
ssc_sensor_proximity_new_finish (GAsyncResult *result, GError **error)
{
	GObject *sensor;
	GObject *source;

	g_debug ("*** ssc_sensor_proximity_new_finish ***");

	source = g_async_result_get_source_object (result);
	sensor = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);
	g_object_unref (source);

	if (!sensor)
		return NULL;

	return SSC_SENSOR_PROXIMITY (sensor);
}

void
ssc_sensor_proximity_new (GFile *file, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_debug ("*** ssc_sensor_proximity_new ***");
	g_return_if_fail (G_IS_FILE (file));

	g_debug ("IS FILE");
	g_async_initable_new_async (
		SSC_TYPE_SENSOR_PROXIMITY,
		G_PRIORITY_DEFAULT,
		cancellable,
		callback,
		user_data,
		SSC_SENSOR_DATA_TYPE, "proximity",
		SSC_CLIENT_FILE_PATH, file,
		NULL);
}
