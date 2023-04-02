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

#include "libssc-sensor-suid.h"

SSCSensor *
suid_handle_report (guint32 msg_id, guint64 uid_low, guint64 uid_high, GArray *protobuf)
{
	SSCSensor 	*sensor = NULL;
	SscSuidResponse *msg = NULL;

	/* Filter out any reports that are not for the SUID sensor */
	if (uid_low != SUID_SENSOR_UID_LOW || uid_high != SUID_SENSOR_UID_HIGH || msg_id != SUID_SENSOR_MSG_EVENT)
		return sensor;

	/* Retrieve sensor UID from Protobuf */
	msg = ssc_suid_response__unpack (NULL, protobuf->len, (const uint8_t *) protobuf->data);

	/*
	 * Only the first discovered sensor is reported which is fine because
	 * we ask for the default sensor for the data type.
	 */
	if (msg->uid != NULL && msg->n_uid > 0) {
		sensor = g_slice_new (SSCSensor);
		sensor->data_type = g_strdup (msg->data_type);
		sensor->uid_low = msg->uid[0]->low;
		sensor->uid_high = msg->uid[0]->high;
	}

	ssc_suid_response__free_unpacked (msg, NULL);

	return sensor;
}

static void
handle_report (GTask *task, GArray *protobuf)
{
	SscClientResponse *msg;
	SSCSensor 	  *sensor = NULL;

	/* Print raw Protobuf data in debug mode */
	g_debug("suid: lookup: ProtoBuf data:");
	for (guint i = 0; i < protobuf->len; i++) {
		guint8 value = g_array_index (protobuf, guint8, i);
		g_print ("\\x%02x", value);
	}
	g_printf("\n");

	msg = ssc_client_response__unpack (NULL, protobuf->len, (const uint8_t *) protobuf->data);

	for (gsize i = 0; i < msg->n_response; i++) {
		SscClientResponseBody *body = msg->response[i];
		GArray *buf = g_array_new (FALSE, FALSE, 1);
		g_array_set_size (buf, body->msg.len);
		buf->data = (char *) body->msg.data;
		sensor = suid_handle_report (body->msg_id, msg->uid->low, msg->uid->high, buf);
	}

	ssc_client_response__free_unpacked (msg, NULL);

	g_task_return_pointer (task, sensor, NULL); // TODO: handle destroy
	g_clear_object (&task);
}

static void
report_large_received (QmiClientSsc *client, QmiIndicationSscReportLargeOutput *output, gpointer user_data)
{
	GTask *task = user_data;
	GArray *protobuf = NULL;
	g_autoptr (GError) error = NULL;

	if (!qmi_indication_ssc_report_large_output_get_protobuf_data (output, &protobuf, &error)) {
		g_warning ("suid: lookup: cannot extract Protobuf data (large): %s\n", error->message);
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	handle_report (task, protobuf);
}

static void
report_small_received (QmiClientSsc *client, QmiIndicationSscReportSmallOutput *output, gpointer user_data)
{
	GTask *task = user_data;
	GArray *protobuf = NULL;
	g_autoptr (GError) error = NULL;

	if (!qmi_indication_ssc_report_small_output_get_protobuf_data (output, &protobuf, &error)) {
		g_warning ("suid: lookup: cannot extract Protobuf data (small): %s\n", error->message);
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	handle_report (task, protobuf);
}

static void
suid_lookup_ready (QmiClientSsc *qmi_client_ssc, GAsyncResult *res, gpointer user_data)
{
	QmiMessageSscControlOutput *output = NULL;
	g_autoptr (GError) 	    error = NULL;
	GTask                      *task = user_data;

	/* Process QMI response */
	output = qmi_client_ssc_control_finish (qmi_client_ssc, res, &error);
	if (!output) {
		qmi_message_ssc_control_output_unref (output);
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	if (!qmi_message_ssc_control_output_get_result (output, &error)) {
		g_warning ("suid: lookup: request failed: %s", error->message);
		qmi_message_ssc_control_output_unref (output);
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	qmi_message_ssc_control_output_unref (output);
}

static void
suid_lookup (SSCClient *self, GTask *task)
{
	QmiMessageSscControlInput *input = NULL;
	g_autoptr (GError)	   error = NULL;
	gchar			  *data_type = NULL;
	GArray                    *buf = NULL;
	GArray			  *suid = NULL;
	SscSuidRequest		   suid_msg;
	SscClientRequestBody       body_msg;
	SscClientConfig            config_msg;
	SscClientRequest           client_msg;
	SscUid                     uid_msg;

	buf = g_array_new (FALSE, FALSE, 1);
	suid = g_array_new (FALSE, FALSE, 1);
	data_type = g_task_get_task_data (task);

	/*
	 * Build Protobuf message:
	 *  - Only get the default sensor for the data_type. 
	 *  - Ignore new sensors for the data_type.
	 */
	ssc_suid_request__init (&suid_msg);
	suid_msg.data_type = data_type;
	suid_msg.has_enable_updates = true;
	suid_msg.enable_updates = false;
	suid_msg.has_only_default_values = true;
	suid_msg.only_default_values = true;

	ssc_client_config__init (&config_msg);
	config_msg.processor = SSC_PROCESSOR_APSS;
	config_msg.suspend_mode = SSC_SUSPEND_MODE_WAKEUP;

	ssc_client_request_body__init (&body_msg);
	body_msg.has_msg = true;

	g_array_set_size (suid, ssc_suid_request__get_packed_size (&suid_msg));
	ssc_suid_request__pack (&suid_msg, (unsigned char*) suid->data);
	body_msg.msg.data = (unsigned char*) suid->data;
	body_msg.msg.len = suid->len;

	ssc_uid__init (&uid_msg);
	uid_msg.low = SUID_SENSOR_UID_LOW;
	uid_msg.high = SUID_SENSOR_UID_HIGH;

	ssc_client_request__init (&client_msg);
	client_msg.uid = &uid_msg;
	client_msg.msg_id = SUID_SENSOR_MSG_REQUEST;
	client_msg.config = &config_msg;
	client_msg.request = &body_msg;

	g_array_set_size (buf, ssc_client_request__get_packed_size (&client_msg));
	ssc_client_request__pack (&client_msg, (unsigned char*) buf->data);

	if (buf == NULL) {
		/*g_task_return_new_error (task,
					 LIBSSC_ERROR,
					 LIBSSC_ERROR_PROTOBUF,
					 "Protobuf message couldn't be build for SUID sensor");*/
		g_clear_object (&task);
		return;
	}

	/* Build QMI message */
	input = qmi_message_ssc_control_input_new ();

	if (!qmi_message_ssc_control_input_set_unknown_value (input, QMI_REQUEST_UNKNOWN_VALUE, &error)) {
		g_warning ("suid: lookup: inserting unknown value failed: %s", error->message);
		qmi_message_ssc_control_input_unref (input);
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	if (!qmi_message_ssc_control_input_set_protobuf_data (input, buf, &error)) {
		g_warning ("suid: lookup: inserting protobuf data failed: %s", error->message);
		qmi_message_ssc_control_input_unref (input);
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	g_debug ("suid: lookup: sending QMI request");

	g_print ("Protobuf: ");
	for (guint i = 0; i < buf->len; i++) {
		guint8 value = g_array_index (buf, guint8, i);
		g_print ("\\x%02x", value);
	}
	g_printf("\n");

	/* Connecting signals for QMI indication with Protobuf response */
	//self->indication_report_small_id 
	guint id = g_signal_connect (self->qmi_client_ssc,
			"report-small",
			G_CALLBACK (report_small_received),
			task);
	//self->indication_report_large_id 
	guint id2 = g_signal_connect (self->qmi_client_ssc,
			"report-large",
			G_CALLBACK (report_large_received),
			task);

	g_debug ("suid: lookup: registered signals for QMI indication with Protobuf response");

	/* Send QMI message with Protobuf payload */
	qmi_client_ssc_control (self->qmi_client_ssc,
		input,
		10,
		NULL,
		(GAsyncReadyCallback)suid_lookup_ready,
		task);

	qmi_message_ssc_control_input_unref (input);
}

static void
suid_lookup_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	SSCClient *self = source_object;

	g_task_set_check_cancellable (task, FALSE);
	suid_lookup (self, task);
}

void
ssc_sensor_suid_lookup (SSCClient *self, gchar *data_type, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	
	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_task_data (task, data_type, NULL);
	g_task_set_check_cancellable (task, FALSE);
	suid_lookup (self, task);
}

SSCSensor *
ssc_sensor_suid_lookup_finish (SSCClient *self, GAsyncResult *res, GError **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

SSCSensor *
ssc_sensor_suid_lookup_sync (SSCClient *self, gchar *data_type, GError **error)
{
	GTask *task = NULL;
	SSCSensor *sensor = NULL;
	
	task = g_task_new (self, NULL, NULL, NULL);
	g_task_set_task_data (task, data_type, NULL);
	g_task_run_in_thread_sync (task, suid_lookup_thread);

	sensor = g_task_propagate_pointer (task, error);
	g_clear_object (&task);
	return sensor;
}
