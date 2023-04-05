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

/*****************************************************************************/

void
proximity_handle_report (SSCClient *self, guint32 msg_id, guint64 uid_low, guint64 uid_high, GArray *protobuf)
{
	SSCSensor *sensor = NULL;
	SscProximityResponse *msg = NULL;
	gboolean near = false;

	sensor = ssc_client_get_sensor_by_data_type (self, SSC_SENSOR_TYPE_PROXIMITY); 

	/* Filter out any reports that are not for the proximity sensor */
	if (uid_low != sensor->uid_low || uid_high != sensor->uid_high || msg_id != SSC_MSG_RESPONSE_PROXIMITY)
		return;

	/* Retrieve near/far value from Protobuf */
	msg = ssc_proximity_response__unpack (NULL, protobuf->len, (const uint8_t *) protobuf->data);

	/* Ignore unreliable measurements */
	if (msg->accuracy == SSC_ACCURACY_UNRELIABLE) {
		g_debug ("Reject unreliable proximity measurement");
		ssc_proximity_response__free_unpacked (msg, NULL);
		return;
	}

	switch (msg->near) {
		case SSC_SENSOR_PROXIMITY_NEAR:
			near = true;
			break;
		case SSC_SENSOR_PROXIMITY_FAR:
			near = false;
			break;
		default:
			g_assert_not_reached();
	}
	g_debug ("Proximity sensor measurement: %s", near? "near" : "far");

	ssc_proximity_response__free_unpacked (msg, NULL);

	/* Emit signal with proximity data */
	g_signal_emit (self, signals[SIGNAL_MEASUREMENT_PROXIMITY], sensor->uid_high, sensor->uid_low, near);
}

/*****************************************************************************/

static void
proximity_open_ready (SSCClient *self, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GError *error = NULL;
	SSCSensor *sensor = NULL;

	sensor = ssc_client_get_sensor_by_data_type (self, SSC_SENSOR_TYPE_PROXIMITY);

	if (ssc_client_send_finish (self, result, &error)) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	/* Sensor enabled successfully, start handling sensor measurement reports */
	sensor->report_id = g_signal_connect (self,
		"report",
		G_CALLBACK (proximity_handle_report),
		NULL);

	g_task_return_boolean (task, TRUE);
	g_clear_object (&task);
}

static void
proximity_open (SSCClient *self, GTask *task)
{
	g_autoptr (GError)  error = NULL;
	SSCSensor *sensor = NULL;

	sensor = ssc_client_get_sensor_by_data_type (self, SSC_SENSOR_TYPE_PROXIMITY); 

	g_debug ("Sending request to enable proximity sensor");

	ssc_client_send (self,
			 sensor,
			 SSC_MSG_REQUEST_ENABLE_REPORT_ON_CHANGE,
			 NULL,
			 NULL,
			 (GAsyncReadyCallback)proximity_open_ready,
			 task);

}

static void
proximity_open_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	SSCClient *self = source_object;

	g_task_set_check_cancellable (task, FALSE);
	proximity_open (self, task);
}

SSCSensor *get_proximity_sensor (SSCClient *self) {
	return NULL;
}

void
ssc_sensor_proximity_open (SSCClient *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_check_cancellable (task, FALSE);

	proximity_open (self, task);
}

gboolean
ssc_sensor_proximity_open_finish (SSCClient *self, GAsyncResult *res, GError **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

gboolean
ssc_sensor_proximity_open_sync (SSCClient *self, gchar *data_type, GError **error)
{
	GTask *task = NULL;
	gboolean success = false;
	
	task = g_task_new (self, NULL, NULL, NULL);
	g_task_run_in_thread_sync (task, proximity_open_thread);

	success = g_task_propagate_boolean (task, error);
	g_clear_object (&task);

	return success;
}

/*****************************************************************************/

static void
proximity_close_ready (SSCClient *self, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	GError *error = NULL;
	SSCSensor *sensor = NULL;

	sensor = ssc_client_get_sensor_by_data_type (self, SSC_SENSOR_TYPE_PROXIMITY);

	if (ssc_client_send_finish (self, result, &error)) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	/* Sensor disabled successfully, stop handling sensor measurement reports */
	g_signal_handler_disconnect (self, sensor->report_id);

	g_task_return_boolean (task, TRUE);
	g_clear_object (&task);
}

static void
proximity_close (SSCClient *self, GTask *task)
{
	g_autoptr (GError)  error = NULL;
	SSCSensor *sensor = NULL;

	sensor = ssc_client_get_sensor_by_data_type (self, SSC_SENSOR_TYPE_PROXIMITY); 

	g_debug ("Sending request to disable proximity sensor");

	ssc_client_send (self,
			 sensor,
			 SSC_MSG_REQUEST_DISABLE_REPORT,
			 NULL,
			 NULL,
			 (GAsyncReadyCallback)proximity_close_ready,
			 task);

}

static void
proximity_close_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	SSCClient *self = source_object;

	g_task_set_check_cancellable (task, FALSE);
	proximity_close (self, task);
}

void
ssc_sensor_proximity_close (SSCClient *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_check_cancellable (task, FALSE);

	proximity_close (self, task);
}

gboolean
ssc_sensor_proximity_close_finish (SSCClient *self, GAsyncResult *res, GError **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

gboolean
ssc_sensor_proximity_close_sync (SSCClient *self, gchar *data_type, GError **error)
{
	GTask *task = NULL;
	gboolean success = false;
	
	task = g_task_new (self, NULL, NULL, NULL);
	g_task_run_in_thread_sync (task, proximity_close_thread);

	success = g_task_propagate_boolean (task, error);
	g_clear_object (&task);

	return success;
}

