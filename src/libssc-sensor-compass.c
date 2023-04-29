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
	guint measurement_accelerometer_id;
	guint measurement_magnetometer_id;
	guint measurement_compass_id;
	GMainContext *context;
	GThread *thread;
	GMainLoop *loop;
	SSCSensorAccelerometer *accelerometer;
	SSCSensorMagnetometer *magnetometer;
	gdouble accel_x;
	gdouble accel_y;
	gdouble accel_z;
	gdouble mag_x;
	gdouble mag_y;
	gdouble mag_z;
	GMutex *lock;
} SSCSensorCompassPrivate;

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SSCSensorCompass, ssc_sensor_compass, SSC_TYPE_SENSOR,
			 G_ADD_PRIVATE (SSCSensorCompass)
			 G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

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

static gpointer
report_receiver_thread (gpointer user_data)
{
	SSCSensorCompass *self = SSC_SENSOR_COMPASS (user_data);
	SSCSensorCompassPrivate *priv = NULL;

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

	g_signal_handler_disconnect (priv->accelerometer, priv->measurement_accelerometer_id);
	g_signal_handler_disconnect (priv->magnetometer, priv->measurement_magnetometer_id);

	g_main_context_pop_thread_default (priv->context);

	return NULL;
}

static void
measurement_accelerometer_cb (SSCSensorAccelerometer *sensor, gfloat accel_x, gfloat accel_y, gfloat accel_z, gpointer user_data)
{
	SSCSensorCompass *self = SSC_SENSOR_COMPASS (user_data);
	SSCSensorCompassPrivate *priv = NULL;

	priv = ssc_sensor_compass_get_instance_private (self);

	g_mutex_lock (priv->lock);

	priv->accel_x = accel_x;
	priv->accel_y = accel_y;
	priv->accel_z = accel_z;

	g_mutex_unlock (priv->lock);
}

static void
measurement_magnetometer_cb (SSCSensorMagnetometer *sensor, gfloat mag_x, gfloat mag_y, gfloat mag_z, gpointer user_data)
{
	SSCSensorCompass *self = SSC_SENSOR_COMPASS (user_data);
	SSCSensorCompassPrivate *priv = NULL;

	priv = ssc_sensor_compass_get_instance_private (self);

	g_mutex_lock (priv->lock);

	priv->mag_x = mag_x;
	priv->mag_y = mag_y;
	priv->mag_z = mag_z;

	g_mutex_unlock (priv->lock);
}

static gboolean
measurement_compass_cb (gpointer user_data)
{
	SSCSensorCompass *self = SSC_SENSOR_COMPASS (user_data);
	SSCSensorCompassPrivate *priv = NULL;
	gdouble heading;
	gdouble pitch;
	gdouble roll;
	gdouble horizontal_x;
	gdouble horizontal_y;
	
	priv = ssc_sensor_compass_get_instance_private (self);

	g_mutex_lock (priv->lock);

	/* Calculate roll and pitch angle from accelerometer */
	pitch = asin (priv->accel_x / SSC_SENSOR_COMPASS_GRAVITY);
	roll = atan (priv->accel_y / priv->accel_z);

	/* Tilt-compensate the magnetometer readings with pitch and roll angles */
	horizontal_x = priv->mag_x * cos (pitch) + priv->mag_z * sin (pitch);
	horizontal_y = priv->mag_x * sin (roll) + priv->mag_y * cos (roll) - priv->mag_z * sin (roll) * cos (pitch);

	/* Calculate tilt-compensated heading of our compass */
	heading = atan2 (horizontal_y, horizontal_x);

	g_mutex_unlock (priv->lock);

	g_signal_emit (self, signals[SIGNAL_MEASUREMENT], 0, heading);

	return G_SOURCE_CONTINUE;
}

/*****************************************************************************/

gboolean
ssc_sensor_compass_close_finish (SSCSensorCompass *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
magnetometer_close_ready (SSCSensorMagnetometer *magnetometer, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;
	GTask *task = G_TASK (user_data);

	if (!ssc_sensor_magnetometer_close_finish (magnetometer, result, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	/* Sensor closing finished */
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
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

	if (!ssc_sensor_accelerometer_close_finish (accelerometer, result, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	/* Close magnetometer */
	ssc_sensor_magnetometer_close (priv->magnetometer, NULL, (GAsyncReadyCallback)magnetometer_close_ready, task);
}

void
ssc_sensor_compass_close (SSCSensorCompass *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensorCompassPrivate *priv = NULL;

	priv = ssc_sensor_compass_get_instance_private (self);
	task = g_task_new (self, cancellable, callback, user_data);

	/* Stop compass calculation */
	g_source_remove (priv->measurement_compass_id);
	priv->measurement_compass_id = 0;

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

gboolean
ssc_sensor_compass_open_finish (SSCSensorCompass *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
magnetometer_open_ready (SSCSensorMagnetometer *magnetometer, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;
	GTask *task = G_TASK (user_data);
	SSCSensorCompass *self = NULL;
	SSCSensorCompassPrivate *priv = NULL;

	self = g_task_get_source_object (task);
	priv = ssc_sensor_compass_get_instance_private (self);

	if (!ssc_sensor_magnetometer_open_finish (magnetometer, result, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	/* Start compass calculation */
	priv->measurement_compass_id = g_timeout_add (SSC_COMPASS_MEASUREMENT_INTERVAL,
						      (GSourceFunc)measurement_compass_cb,
						      self);

	/* Sensor opening finished */
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

	if (!ssc_sensor_accelerometer_open_finish (accelerometer, result, &error)) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	/* Open magnetometer */
	ssc_sensor_magnetometer_open (priv->magnetometer, NULL, (GAsyncReadyCallback)magnetometer_open_ready, task);
}

void
ssc_sensor_compass_open (SSCSensorCompass *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensorCompassPrivate *priv = NULL;

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
magnetometer_ready (SSCSensorMagnetometer *magnetometer, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	SSCSensorCompassPrivate *priv = NULL;
	g_autoptr (GError) error = NULL;
	SSCSensorCompass *self = NULL;

	self = g_task_get_source_object (task);
	priv = ssc_sensor_compass_get_instance_private (self);

	/* Magnetometer allocation */
	priv->magnetometer = ssc_sensor_magnetometer_new_finish (result, &error);
	if (!priv->magnetometer) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	/* Start listening for measurement signals */
	priv->measurement_magnetometer_id = g_signal_connect (priv->magnetometer,
			"measurement",
			G_CALLBACK (measurement_magnetometer_cb),
			self);

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

static void
accelerometer_ready (SSCSensorAccelerometer *accelerometer, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	SSCSensorCompassPrivate *priv = NULL;
	g_autoptr (GError) error = NULL;
	SSCSensorCompass *self = NULL;

	self = g_task_get_source_object (task);
	priv = ssc_sensor_compass_get_instance_private (self);

	/* Accelerometer allocation */
	priv->accelerometer = ssc_sensor_accelerometer_new_finish (result, &error);
	if (!priv->accelerometer) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	/* Start listening for report signals */
	priv->measurement_accelerometer_id = g_signal_connect (priv->accelerometer,
			"measurement",
			G_CALLBACK (measurement_accelerometer_cb),
			self);

	ssc_sensor_magnetometer_new (NULL, (GAsyncReadyCallback)magnetometer_ready, task);
}

static void
initable_init_async (GAsyncInitable *initable, int io_priority, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensorCompass *self = NULL;

	self = SSC_SENSOR_COMPASS (initable);
	task = g_task_new (self, cancellable, callback, user_data);

	ssc_sensor_accelerometer_new (NULL, (GAsyncReadyCallback)accelerometer_ready, task);
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
}

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
	SSCSensorCompassPrivate *priv = NULL;

	priv = ssc_sensor_compass_get_instance_private (self);
	priv->accel_x = 0.0;
	priv->accel_y = 0.0;
	priv->accel_z = 0.0;
	priv->mag_x = 0.0;
	priv->mag_y = 0.0;
	priv->mag_z = 0.0;
	g_mutex_init (priv->lock);
}

SSCSensorCompass *
ssc_sensor_compass_new_finish (GAsyncResult *result, GError **error)
{
	GObject *sensor;
	GObject *source;

	source = g_async_result_get_source_object (result);
	sensor = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);

	if (!sensor) {
		g_object_unref (source);
		return NULL;
	}

	g_object_unref (source);
	return SSC_SENSOR_COMPASS (sensor);
}

void
ssc_sensor_compass_new (GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_async_initable_new_async (
			SSC_TYPE_SENSOR_COMPASS,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
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
