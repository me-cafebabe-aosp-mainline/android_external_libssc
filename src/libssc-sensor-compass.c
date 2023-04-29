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

#include "libssc-sensor-compass.h"

#define SSC_COMPASS_MEASUREMENT_INTERVAL 500

enum {
	SIGNAL_MEASUREMENT,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

typedef struct _SSCSensorCompassPrivate {
	guint report_id;
	guint measurement_accelerometer_id;
	SSCSensorAccelerometer *accelerometer;
	GMainContext *context;
	GThread *thread;
	GMainLoop *loop;
	GMutex lock;
	gfloat accel_x;
	gfloat accel_y;
	gfloat accel_z;
} SSCSensorCompassPrivate;

G_DEFINE_TYPE_WITH_CODE (SSCSensorCompass, ssc_sensor_compass, SSC_TYPE_SENSOR,
			 G_ADD_PRIVATE (SSCSensorCompass))

typedef struct {
	GAsyncResult *result;
	GMainLoop *loop;
} SyncContext;

static void
sync_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	SyncContext *ctx = user_data;

	ctx->result = g_object_ref (result);
	g_main_loop_quit (ctx->loop);
}

/*****************************************************************************/

static void
measurement_accelerometer_cb (SSCSensorAccelerometer *sensor, gfloat accel_x, gfloat accel_y, gfloat accel_z, gpointer user_data)
{
	SSCSensorCompass *self = SSC_SENSOR_COMPASS (user_data);
	SSCSensorCompassPrivate *priv = NULL;

	priv = ssc_sensor_compass_get_instance_private (self);

	g_mutex_lock (&priv->lock);

	g_debug ("ACCELEROMETER: X=%f Y=%f Z=%f", accel_x, accel_y, accel_z);
	priv->accel_x = accel_x;
	priv->accel_y = accel_y;
	priv->accel_z = accel_z;

	g_mutex_unlock (&priv->lock);
}

/*****************************************************************************/

static gpointer
report_receiver_thread (gpointer user_data)
{
	SSCSensorCompass *self = SSC_SENSOR_COMPASS (user_data);
	SSCSensorCompassPrivate *priv = NULL;
	SSCClient *client = NULL;

	priv = ssc_sensor_compass_get_instance_private (self);
	g_warn_if_fail (priv->context);

	/*
	 * Create main loop with context to receive QMI indications.
	 * The loop will be quited in close_sync when the thread should exit.
	 * Once quited, disconnect signal handler.
	 */
	g_main_context_push_thread_default (priv->context);

	priv->loop = g_main_loop_new (priv->context, TRUE);
	g_main_loop_run (priv->loop);

	g_object_get (SSC_SENSOR (self),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);
	g_signal_handler_disconnect (client, priv->report_id);
	g_signal_handler_disconnect (priv->accelerometer, priv->measurement_accelerometer_id);

	g_main_context_pop_thread_default (priv->context);

	return NULL;
}

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SscMagnetometerResponse *msg = NULL;
	SSCSensorCompass *sensor = SSC_SENSOR_COMPASS (user_data);
	SSCSensorCompassPrivate *priv = NULL;
	guint64 sensor_uid_low;
	guint64 sensor_uid_high;
	gfloat mag_x;
	gfloat mag_y;
	gfloat mag_z;
	gfloat heading;
	gfloat heading2;
	gfloat pitch;
	gfloat roll;
	gfloat horizontal_x;
	gfloat horizontal_y;

	g_object_get (sensor,
		      SSC_SENSOR_UID_HIGH, &sensor_uid_high,
		      SSC_SENSOR_UID_LOW, &sensor_uid_low,
		      NULL);

	if (sensor_uid_high == uid_high && sensor_uid_low == uid_low && msg_id == SSC_MSG_REPORT_MEASUREMENT_MAGNETOMETER) {
		priv = ssc_sensor_compass_get_instance_private (sensor);

		msg = ssc_magnetometer_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);
		g_mutex_lock (&priv->lock);

		if (msg->n_magnetic_field >= 3) {
			//mag_x = msg->magnetic_field[0];
			//mag_y = msg->magnetic_field[1];
			//mag_z = msg->magnetic_field[2];
			mag_x = msg->magnetic_field[0] / 10.0;
			mag_y = msg->magnetic_field[1] / -10.0;
			mag_z = msg->magnetic_field[2] / 10.0;
		}

		g_debug ("MAGNETOMETER: X=%f, Y=%f, Z=%f", mag_x, mag_y, mag_z);

		ssc_magnetometer_response__free_unpacked (msg, NULL);

		/* Calculate roll and pitch angle from accelerometer */
		//pitch = asin (priv->accel_x / SSC_SENSOR_COMPASS_GRAVITY);
		//roll = atan (priv->accel_y / priv->accel_z);
		roll = asin (priv->accel_x / SSC_SENSOR_COMPASS_GRAVITY);
		pitch = atan (priv->accel_y / priv->accel_z);
		//pitch = asin (-priv->accel_x);
		//roll = asin (priv->accel_y / cos (pitch));
		//g_printf ("B] PITCH: %f degrees | ROLL: %f degrees", pitch * 180.0 / M_PI, roll * 180.0 / M_PI);

		/* Tilt-compensate the magnetometer readings with pitch and roll angles */
		horizontal_x = mag_x * cos (pitch) + mag_z * sin (pitch);
		horizontal_y = mag_x * sin (roll) + mag_y * cos (roll) - mag_z * sin (roll) * cos (pitch);

		/* Calculate tilt-compensated heading of our compass */
		heading = atan2 (horizontal_y, horizontal_x) * 180.0 / M_PI;
		if (heading < 0)
			heading += 360.0;
		heading2 = atan2 (mag_y, mag_x);
		if (heading < 0)
			heading += 360.0;


		g_printf ("PITCH: %f ° | ROLL: %f °\n | HEADING: %f | HEADING2: %f°\n", pitch * 180.0 / M_PI, roll * 180.0 / M_PI, heading, heading2);
		g_mutex_unlock (&priv->lock);

		g_signal_emit (sensor, signals[SIGNAL_MEASUREMENT], 0, heading);
	}
}

/*****************************************************************************/

static void
compass_close_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_compass_parent_class)->close_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ssc_sensor_compass_close_finish (SSCSensorCompass *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
accelerometer_close_ready (SSCSensorAccelerometer *accelerometer, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;
	GTask *task = G_TASK (user_data);
	SSCSensorCompass *self = NULL;
	SSCSensorCompassPrivate *priv = NULL;

	self = g_task_get_source_object (task);
	priv = ssc_sensor_compass_get_instance_private (self);

	if (!ssc_sensor_accelerometer_close_finish (priv->accelerometer, result, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	/* Close magnetometer */
	SSC_SENSOR_CLASS (ssc_sensor_compass_parent_class)->close (SSC_SENSOR (self), NULL, (GAsyncReadyCallback)compass_close_ready, task);
}

void
ssc_sensor_compass_close (SSCSensorCompass *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensorCompassPrivate *priv = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_compass_parent_class)->close &&
		  SSC_SENSOR_CLASS (ssc_sensor_compass_parent_class)->close_finish);

	priv = ssc_sensor_compass_get_instance_private (self);

	task = g_task_new (self, cancellable, callback, user_data);

	/* Close accelerometer */
	ssc_sensor_accelerometer_close (priv->accelerometer, NULL, (GAsyncReadyCallback)accelerometer_close_ready, task);
}

gboolean
ssc_sensor_compass_close_sync (SSCSensorCompass *self, GCancellable *cancellable, GError **error)
{
	SSCSensorCompassPrivate *priv = NULL;
	gboolean success = FALSE;
	SyncContext ctx;

	priv = ssc_sensor_compass_get_instance_private (self);
	g_warn_if_fail (priv->loop);
	g_warn_if_fail (priv->thread);

	/* Stop report context thread before re-acquiring our context */
	g_main_loop_quit (priv->loop);
	g_thread_join (priv->thread);

	/* Take over context and close sensor */
	g_main_context_push_thread_default (priv->context);
	ctx.loop = g_main_loop_new (priv->context, TRUE);

	ssc_sensor_compass_close (self, cancellable, sync_cb, &ctx);
	g_main_loop_run (ctx.loop);
	success = ssc_sensor_compass_close_finish (self, ctx.result, error);

	g_main_context_pop_thread_default (priv->context);

	return success;
}

/*****************************************************************************/

static void
compass_open_ready (SSCSensor *sensor, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (!SSC_SENSOR_CLASS (ssc_sensor_compass_parent_class)->open_finish (sensor, result, &error)) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

static void
accelerometer_open_ready (SSCSensorAccelerometer *accelerometer, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;
	GTask *task = G_TASK (user_data);
	SSCSensorCompass *self = NULL;
	SSCSensorCompassPrivate *priv = NULL;

	self = g_task_get_source_object (task);
	priv = ssc_sensor_compass_get_instance_private (self);

	if (!ssc_sensor_accelerometer_open_finish (priv->accelerometer, result, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	/* Open magnetometer */
	SSC_SENSOR_CLASS (ssc_sensor_compass_parent_class)->open (SSC_SENSOR (self), NULL, (GAsyncReadyCallback)compass_open_ready, task);
}

gboolean
ssc_sensor_compass_open_finish (SSCSensorCompass *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

void
ssc_sensor_compass_open (SSCSensorCompass *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensorCompassPrivate *priv = NULL;

	g_assert (SSC_SENSOR_CLASS (ssc_sensor_compass_parent_class)->open &&
		  SSC_SENSOR_CLASS (ssc_sensor_compass_parent_class)->open_finish);

	priv = ssc_sensor_compass_get_instance_private (self);

	task = g_task_new (self, cancellable, callback, user_data);

	/* Open accelerometer */
	ssc_sensor_accelerometer_open (priv->accelerometer, NULL, (GAsyncReadyCallback)accelerometer_open_ready, task);
}

gboolean
ssc_sensor_compass_open_sync (SSCSensorCompass *self, GCancellable *cancellable, GError **error)
{
	SSCSensorCompassPrivate *priv = NULL;
	SyncContext ctx;
	gboolean success = FALSE;

	priv = ssc_sensor_compass_get_instance_private (self);

	/* Open sensor in our context */
	g_main_context_push_thread_default (priv->context);
	ctx.loop = g_main_loop_new (priv->context, TRUE);

	ssc_sensor_compass_open (self, cancellable, sync_cb, &ctx);
	g_main_loop_run (ctx.loop);
	success = ssc_sensor_compass_open_finish (self, ctx.result, error);

	/* Start report thread to watch for incoming measurements over QMI indications */
	priv->thread = g_thread_new ("report-receiver-compass", report_receiver_thread, self);

	g_main_context_pop_thread_default (priv->context);

	return success;
}

/*****************************************************************************/

static void
ssc_sensor_compass_class_init (SSCSensorCompassClass *klass)
{
	signals[SIGNAL_MEASUREMENT] = g_signal_new ("measurement",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE,
		1, G_TYPE_FLOAT);
}

static void
ssc_sensor_compass_init (SSCSensorCompass *self)
{
	g_autoptr (GError) error = NULL;
	SSCSensorCompassPrivate *priv = NULL;
	SSCSensorAccelerometer *accelerometer = NULL;

	priv = ssc_sensor_compass_get_instance_private (self);
	g_mutex_init (&priv->lock);
	accelerometer = ssc_sensor_accelerometer_new_sync (NULL, &error);
	priv->accelerometer = error ? NULL : accelerometer;
}

SSCSensorCompass *
ssc_sensor_compass_new_finish (GAsyncResult *result, GError **error)
{
	SSCSensorCompassPrivate *priv = NULL;
	SSCClient *client = NULL;
	GObject *sensor;
	GObject *source;

	source = g_async_result_get_source_object (result);
	sensor = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);

	if (!sensor) {
		g_object_unref (source);
		return NULL;
	}

	priv = ssc_sensor_compass_get_instance_private (SSC_SENSOR_COMPASS (sensor));

	/* Check if accelerometer is available */
	if (!priv->accelerometer) {
		g_warning ("Compass initialization failed: accelerometer unavailable");
		g_object_unref (sensor);
		g_object_unref (source);
		return NULL;
	}

	/* Start listening for reports */
	g_object_get (SSC_SENSOR (sensor),
		      SSC_SENSOR_CLIENT, &client,
		      NULL);
	// TODO: crash because we have not chained up the parent init stuf
	g_debug ("REPORT ID");
	priv->report_id = g_signal_connect (client,
			"report",
			G_CALLBACK (report_received),
			SSC_SENSOR_COMPASS (sensor));
	g_debug ("ACCELEROMETER");
	priv->measurement_accelerometer_id = g_signal_connect (priv->accelerometer,
			"measurement",
			G_CALLBACK (measurement_accelerometer_cb),
			SSC_SENSOR_COMPASS (sensor));
	g_debug ("DONE");

	g_object_unref (source);
	return SSC_SENSOR_COMPASS (sensor);
}

void
ssc_sensor_compass_new (GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	/* Compass sensor consists of magnetometer and accelerometer */
	g_async_initable_new_async (
			SSC_TYPE_SENSOR_COMPASS,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
			SSC_SENSOR_DATA_TYPE, "mag",
			NULL);
}

SSCSensorCompass *
ssc_sensor_compass_new_sync (GCancellable *cancellable, GError **error)
{
	SSCSensorCompass *self = NULL;
	SSCSensorCompassPrivate *priv = NULL;
	SyncContext ctx;
	GMainContext *context = NULL;

	/* Initiate context for this sensor in library */
	context = g_main_context_new ();
	g_main_context_push_thread_default (context);
	ctx.loop = g_main_loop_new (context, TRUE);

	/* Create sensor */
	ssc_sensor_compass_new (cancellable, sync_cb, &ctx);
	g_main_loop_run (ctx.loop);
	self = ssc_sensor_compass_new_finish (ctx.result, error);

	g_main_context_pop_thread_default (context);

	if (!self)
		return NULL;

	/* Keep context for future calls to avoid interference with default context */
	priv = ssc_sensor_compass_get_instance_private (self);
	priv->context = g_main_context_ref (context);

	return self;
}
