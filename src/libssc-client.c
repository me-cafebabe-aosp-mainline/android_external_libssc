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

#include "libssc-client.h"

enum {
	SIGNAL_REPORT,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

enum {
	PROP_FILE = 1,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES];

typedef struct _SSCClientPrivate {
	GFile *file;
	QmiDevice *device;
	QmiClientSsc *qmi_client_ssc;
	QrtrBus *bus;
	guint32 node_id;
	guint indication_report_small_id;
	guint indication_report_large_id;
	guint report_id;
	guint discovery_requests;
	guint attr_requests;
	GArray *sensors;
} SSCClientPrivate;

typedef struct _SSCClient {
	GObject parent;
	SSCClientPrivate *priv;
} SSCClient;

#define SSC_NUMBER_OF_SENSORS 4
static gchar* data_type[SSC_NUMBER_OF_SENSORS] = {
	"accel",
	"mag",
	"proximity",
	"ambient_light",
};

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SSCClient, ssc_client, G_TYPE_OBJECT,
			 G_ADD_PRIVATE (SSCClient)
			 G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init));

/*****************************************************************************/

SSCSensor *
ssc_client_get_sensor_by_data_type (SSCClient *self, gchar *data_type)
{
	SSCSensor *sensor = NULL;
	SSCClientPrivate *priv = NULL;

	priv = ssc_client_get_instance_private (self);

	for (gsize i = 0; i < priv->sensors->len; i++) {
		sensor = &g_array_index (priv->sensors, SSCSensor, i);
		if (sensor == NULL)
			g_error ("Sensor cannot be NULL");
		if (g_strcmp0 (sensor->data_type, data_type) == 0)
			return sensor;
	}

	return NULL;
}

SSCSensor *
ssc_client_get_sensor_by_uid (SSCClient *self, guint64 uid_low, guint64 uid_high)
{
	SSCSensor *sensor = NULL;
	SSCClientPrivate *priv = NULL;

	priv = ssc_client_get_instance_private (self);

	for (gsize i = 0; i < priv->sensors->len; i++) {
		sensor = &g_array_index (priv->sensors, SSCSensor, i);
		if (sensor == NULL)
			g_error ("Sensor cannot be NULL");
		if (sensor->uid_low == uid_low && sensor->uid_high == uid_high)
			return sensor;
	}

	return NULL;
}

/*****************************************************************************/

static void
attr (SSCClient *self, SSCSensor *sensor);

static void
handle_report (SSCClient *self, GArray *protobuf)
{
	SSCClientPrivate *priv = NULL;
	SscClientResponse *msg;
	SscSuidResponse *suid_msg = NULL;
	SscAttrResponse *attr_msg = NULL;

	priv = ssc_client_get_instance_private (self);

	ssc_common_dump_protobuf (protobuf);
	msg = ssc_client_response__unpack (NULL, protobuf->len, (const uint8_t *) protobuf->data);

	for (gsize i = 0; i < msg->n_response; i++) {
		SscClientResponseBody *body = msg->response[i];
		GArray *buf = g_array_new (FALSE, FALSE, 1);
		g_array_set_size (buf, body->msg.len);
		buf->data = (char *) body->msg.data;

		g_debug ("Got message %" G_GUINT32_FORMAT " for sensor %016lX %016lX", body->msg_id, msg->uid->high, msg->uid->low);

		/*
		 * SUID sensor is handled by SSCClient because it is not a regular sensor.
		 * Only the first discovered sensor is reported which is fine because
		 * we ask for the default sensor for the data type.
		 */
		if (msg->uid->low == SSC_SENSOR_UID_SUID_LOW && msg->uid->high == SSC_SENSOR_UID_SUID_HIGH) {
			if (body->msg_id == SSC_MSG_RESPONSE_SUID) {
				suid_msg = ssc_suid_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

				if (suid_msg != NULL) {
					if (suid_msg->n_uid > 0) {
						SSCSensor sensor;
						sensor.data_type = g_strdup (suid_msg->data_type);
						sensor.uid_low = suid_msg->uid[0]->low;
						sensor.uid_high = suid_msg->uid[0]->high;
						g_array_append_val (priv->sensors, sensor);

						g_info ("Populating attributes for '%s' sensor (%016lX %016lX)", sensor.data_type, sensor.uid_high, sensor.uid_low);
						attr (self, &sensor);
					} else
						g_info ("No '%s' sensor available", suid_msg->data_type);

					ssc_suid_response__free_unpacked (suid_msg, NULL);
				} else
					g_warning ("Cannot unpack SUID message response");

			/* We should never end up here because we only have a single response type for the SUID sensor */
			} else 
				g_error ("Unhandled SUID sensor message: %d", body->msg_id);

			continue;
		}
		/*
		 * Sensor attributes are handled by SSCClient, catch these responses and 
		 * update the corresponding SSCSensor. Only a subset of attributes are supported.
		 */
		else if (body->msg_id == SSC_MSG_RESPONSE_GET_ATTRIBUTES) {
			attr_msg = ssc_attr_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

			if (attr_msg != NULL) {
				SSCSensor *sensor = ssc_client_get_sensor_by_uid (self, msg->uid->low, msg->uid->high);

				/* We should always have a sensor here because we just discovered it */
				if (sensor == NULL)
					g_error ("No sensor (%016lX %016lX) available while populating its attributes!", sensor->uid_high, sensor->uid_low);

				for (gsize i = 0; i < attr_msg->n_attr; i++) {
					switch (attr_msg->attr[i]->id) {
						case SSC_ATTRIBUTE_NAME:
							if (attr_msg->attr[i]->value_array->n_v == 1 && attr_msg->attr[i]->value_array->v[0]->s) {
								g_debug ("Attr 'name': %s", attr_msg->attr[i]->value_array->v[0]->s);
								sensor->name = g_strdup (attr_msg->attr[i]->value_array->v[0]->s);
							}
							break;
						case SSC_ATTRIBUTE_VENDOR:
							if (attr_msg->attr[i]->value_array->n_v == 1 && attr_msg->attr[i]->value_array->v[0]->s) {
								g_debug ("Attr 'vendor': %s", attr_msg->attr[i]->value_array->v[0]->s);
								sensor->vendor = g_strdup (attr_msg->attr[i]->value_array->v[0]->s);
							}
							break;
						case SSC_ATTRIBUTE_AVAILABLE:
							if (attr_msg->attr[i]->value_array->n_v == 1 && attr_msg->attr[i]->value_array->v[0]->has_b) {
								g_debug ("Attr 'available': %s", attr_msg->attr[i]->value_array->v[0]->b ? "yes": "no");
								sensor->available = attr_msg->attr[i]->value_array->v[0]->b;
							}
							break;
						case SSC_ATTRIBUTE_STREAM_TYPE:
							if (attr_msg->attr[i]->value_array->n_v == 1 && attr_msg->attr[i]->value_array->v[0]->has_i) {
								g_debug ("Attr 'stream-type': %ld", attr_msg->attr[i]->value_array->v[0]->i);
								sensor->stream_type = attr_msg->attr[i]->value_array->v[0]->i;
							}
							break;
					}
				}

				ssc_attr_response__free_unpacked (attr_msg, NULL);
			} else
				g_warning ("Cannot unpack attribute message response");
		}
		/*
		 * Emit a GSignal on which sensor drivers can subscribe to
		 * receive sensor specific messages. Drivers can emit the sensor data
		 * once they have processed it.
		 */
		g_signal_emit (self, signals[SIGNAL_REPORT], 0, body->msg_id, msg->uid->high, msg->uid->low, buf);
	}

	ssc_client_response__free_unpacked (msg, NULL);
}

static void
report_large_received (QmiClientSsc *self, QmiIndicationSscReportLargeOutput *output, gpointer user_data)
{
	SSCClient *client = user_data;
	g_autoptr (GError) error = NULL;
	GArray *protobuf = NULL;

	if (!qmi_indication_ssc_report_large_output_get_protobuf_data (output, &protobuf, &error)) {
		g_warning ("Cannot extract Protobuf data (large report): %s", error->message);
		return;
	}

	handle_report (client, protobuf);
}

static void
report_small_received (QmiClientSsc *self, QmiIndicationSscReportSmallOutput *output, gpointer user_data)
{
	SSCClient *client = user_data;
	g_autoptr (GError) error = NULL;
	GArray *protobuf = NULL;

	if (!qmi_indication_ssc_report_small_output_get_protobuf_data (output, &protobuf, &error)) {
		g_warning ("Cannot extract Protobuf data (small report): %s", error->message);
		return;
	}

	handle_report (client, protobuf);
}

static void
request_ready (QmiClientSsc *self, GAsyncResult *res, gpointer user_data)
{
	QmiMessageSscControlOutput *output = NULL;
	g_autoptr (GError) 	    error = NULL;
	GTask                      *task = user_data;

	output = qmi_client_ssc_control_finish (self, res, &error);
	if (!output) {
		qmi_message_ssc_control_output_unref (output);
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	if (!qmi_message_ssc_control_output_get_result (output, &error)) {
		g_warning ("QMI request failed: %s", error->message);
		qmi_message_ssc_control_output_unref (output);
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	qmi_message_ssc_control_output_unref (output);
	g_task_return_boolean (task, TRUE);
	g_clear_object (&task);
}

gboolean
ssc_client_send_finish (SSCClient *self, GAsyncResult *res, GError **error)
{
	return g_task_propagate_boolean (G_TASK (res), error);
}

void
ssc_client_send (SSCClient *self, SSCSensor *sensor, guint32 message_id, GArray *protobuf, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask                     *task = NULL;
	QmiMessageSscControlInput *input = NULL;
	g_autoptr (GError)	   error = NULL;
	GArray                    *buf = NULL;
	SscClientRequestBody       body_msg;
	SscClientConfig            config_msg;
	SscClientRequest           client_msg;
	SscUid                     uid_msg;
	SSCClientPrivate	  *priv = NULL;

	task = g_task_new (self, cancellable, callback, user_data);
	buf = g_array_new (FALSE, FALSE, 1);
	priv = ssc_client_get_instance_private (self);

	ssc_client_config__init (&config_msg);
	config_msg.processor = SSC_PROCESSOR_APSS;
	config_msg.suspend_mode = SSC_SUSPEND_MODE_WAKEUP;

	ssc_client_request_body__init (&body_msg);
	if (protobuf) {
		body_msg.has_msg = true;
		body_msg.msg.data = (uint8_t *)protobuf->data;
		body_msg.msg.len = protobuf->len;
	}

	ssc_uid__init (&uid_msg);
	uid_msg.low = sensor->uid_low;
	uid_msg.high = sensor->uid_high;

	ssc_client_request__init (&client_msg);
	client_msg.uid = &uid_msg;
	client_msg.msg_id = message_id;
	client_msg.config = &config_msg;
	client_msg.request = &body_msg;

	g_array_set_size (buf, ssc_client_request__get_packed_size (&client_msg));
	ssc_client_request__pack (&client_msg, (unsigned char*) buf->data);

	if (buf == NULL) {
		g_warning ("Protobuf message couldn't be build for SUID sensor");
		return;
	}
	ssc_common_dump_protobuf (buf);

	/* Build QMI message */
	input = qmi_message_ssc_control_input_new ();

	if (!qmi_message_ssc_control_input_set_unknown_value (input, SSC_QMI_REQUEST_UNKNOWN_VALUE, &error)) {
		g_warning ("Inserting unknown value failed: %s", error->message);
		qmi_message_ssc_control_input_unref (input);
		return;
	}

	if (!qmi_message_ssc_control_input_set_protobuf_data (input, buf, &error)) {
		g_warning ("Inserting protobuf data failed: %s", error->message);
		qmi_message_ssc_control_input_unref (input);
		return;
	}

	/* Send QMI message with Protobuf payload */
	qmi_client_ssc_control (priv->qmi_client_ssc,
		input,
		10,
		NULL,
		(GAsyncReadyCallback)request_ready,
		task);

	qmi_message_ssc_control_input_unref (input);
}

/*****************************************************************************/

static void
discover (SSCClient *self, gchar *data_type, GTask *task);

static void
report_received (SSCClient *self, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	SSCClientPrivate *priv = NULL;

	priv = ssc_client_get_instance_private (self);
	
	/* Still discovering */
	priv->discovery_requests++;
	if (priv->discovery_requests < SSC_NUMBER_OF_SENSORS) {
		g_debug ("Discovering '%s' sensor", data_type[priv->discovery_requests]);
		discover (self, data_type[priv->discovery_requests], task);
		return;
	}

	/* Discovery complete, stop handling reports */
	g_signal_handler_disconnect (self, priv->report_id);
	priv->report_id = 0;

	g_info ("SSC client allocated with %d sensors:", priv->sensors->len);
	for (gsize i = 0; i < priv->sensors->len; i++) {
		SSCSensor *sensor = &g_array_index (priv->sensors, SSCSensor, i);
		g_info ("%" G_GSIZE_FORMAT ". Sensor '%s' (%016lX %016lX)", i + 1, sensor->data_type, sensor->uid_high, sensor->uid_low);
		g_info ("  name: %s", sensor->name);
		g_info ("  vendor: %s", sensor->vendor);
		g_info ("  stream-type: %s", sensor->stream_type == SSC_STREAM_TYPE_CONTINUOUS ? "continuous" : "on-change");
		g_info ("  available: %s", sensor->available ? "yes" : "no");
	}
	g_task_return_boolean (task, TRUE);
	g_clear_object (&task);
}

static void
attr_ready (SSCClient *self, GAsyncResult *result, gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	GTask *task = G_TASK (user_data);

	if (!ssc_client_send_finish (self, result, &error)) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	/* Task completion will happen when sensor attribute population is complete */
	g_debug ("Sensor attribute request sent");
}

static void
attr (SSCClient *self, SSCSensor *sensor)
{
	SscAttrRequest msg;
	GArray *buf = NULL;

	buf = g_array_new (FALSE, FALSE, 1);
	g_debug ("Retrieving attributes for sensor '%s'", sensor->data_type);

	/* Request all attributes for a sensor without watching for updats. */
	ssc_attr_request__init (&msg);
	msg.has_enable_updates = true;
	msg.enable_updates = false;
	g_array_set_size (buf, ssc_attr_request__get_packed_size (&msg));
	ssc_attr_request__pack (&msg, (unsigned char*) buf->data);

	ssc_client_send (self,
			 sensor,
			 SSC_MSG_REQUEST_GET_ATTRIBUTES,
			 buf,
			 NULL,
			 (GAsyncReadyCallback)attr_ready,
			 NULL); 
}

static void
discovery_ready (SSCClient *self, GAsyncResult *result, gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	GTask *task = G_TASK (user_data);

	if (!ssc_client_send_finish (self, result, &error)) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	/* Task completion will happen when sensor discovery is complete */
	g_debug ("Sensor discovery request sent");
}

static void
discover (SSCClient *self, gchar *data_type, GTask *task)
{
	SSCClientPrivate *priv = NULL;
	SscSuidRequest msg;
	SSCSensor *sensor = NULL;
	GArray *buf = NULL;

	priv = ssc_client_get_instance_private (self);
	buf = g_array_new (FALSE, FALSE, 1);
	g_debug ("Discovering sensor UID for data type '%s'", data_type);

	/* SUID sensor is known, but is not a physical sensor so never add it */
	sensor = g_slice_new (SSCSensor);
	sensor->data_type = SSC_SENSOR_TYPE_SUID;
	sensor->uid_low = SSC_SENSOR_UID_SUID_LOW;
	sensor->uid_high = SSC_SENSOR_UID_SUID_HIGH;

	/*
	 * Request for sensors for given datatype, if multiple sensors support a datatype,
	 * only return the default sensor. Do not monitor for hotplugged sensors.
	 */
	ssc_suid_request__init (&msg);
	msg.data_type = data_type;
	msg.has_enable_updates = true;
	msg.enable_updates = false;
	msg.has_only_default_values = true;
	msg.only_default_values = true;
	g_array_set_size (buf, ssc_suid_request__get_packed_size (&msg));
	ssc_suid_request__pack (&msg, (unsigned char*) buf->data);

	/* Start listening for report signals */
	if (priv->report_id == 0) {
		priv->report_id = g_signal_connect (self,
			"report",
			G_CALLBACK (report_received),
			task);
	}

	ssc_client_send (self,
			 sensor,
			 SSC_MSG_REQUEST_SUID,
			 buf,
			 NULL,
			 (GAsyncReadyCallback)discovery_ready,
			 NULL); 
}

static void
allocate_client_ready (QmiDevice *device, GAsyncResult *result, gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	GTask *task = NULL;
	SSCClient *client = NULL;
	SSCClientPrivate *priv = NULL;

	task = G_TASK (user_data);
	client = g_task_get_task_data (task);
	priv = ssc_client_get_instance_private (client);

	priv->qmi_client_ssc = QMI_CLIENT_SSC (qmi_device_allocate_client_finish (device, result, &error));

	if (error) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	/* Connecting signals for QMI indication with Protobuf response */
	g_debug ("Connecting QMI SSC client signals");
	priv->indication_report_small_id = g_signal_connect (priv->qmi_client_ssc,
			"report-small",
			G_CALLBACK (report_small_received),
			client);
	priv->indication_report_large_id = g_signal_connect (priv->qmi_client_ssc,
			"report-large",
			G_CALLBACK (report_large_received),
			client);

	/* Send discover requests for all sensors */
	priv->discovery_requests = 0;
	priv->attr_requests = 0;
	priv->report_id = 0;
	g_debug ("Discovering '%s' sensor", data_type[priv->discovery_requests]);
	discover (client, data_type[priv->discovery_requests], task);
}

static void
device_open_ready (QmiDevice *device, GAsyncResult *result, gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	GTask *task = NULL;

	task = G_TASK (user_data);

	qmi_device_open_finish (device, result, &error);
	if (error) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	g_debug ("QMI device at '%s' ready", qmi_device_get_path_display (device));

	/* QMI device opened, allocate client */
	qmi_device_allocate_client (device,
		QMI_SERVICE_SSC,
		QMI_CID_NONE,
		10,
		NULL,
		(GAsyncReadyCallback)allocate_client_ready,
		task);
}

static void
device_new_ready (GObject *source, GAsyncResult *res, gpointer user_data)
{
	QmiDeviceOpenFlags open_flags = QMI_DEVICE_OPEN_FLAGS_NONE;
	g_autoptr(GError) error = NULL;
	GTask *task = NULL;
	SSCClient *client = NULL;
	SSCClientPrivate *priv = NULL;

	task = G_TASK (user_data);
	client = g_task_get_task_data (task);
	priv = ssc_client_get_instance_private (client);

	priv->device = qmi_device_new_finish (res, &error);
	if (error) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	/* Indications are expected as they report all sensor data values */
	open_flags |= QMI_DEVICE_OPEN_FLAGS_AUTO;
	open_flags |= QMI_DEVICE_OPEN_FLAGS_EXPECT_INDICATIONS;
	
	g_debug ("QMI device ready");

	/* QMI device created, open device */
	qmi_device_open (priv->device,
		open_flags,
		15,
		NULL,
		(GAsyncReadyCallback)device_open_ready,
		task);
}

static void
bus_new_ready (GObject *source, GAsyncResult *res, gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	QrtrNode *node;
	GTask *task = NULL;
	SSCClient *client = NULL;
	SSCClientPrivate *priv = NULL;

	task = G_TASK (user_data);
	client = g_task_get_task_data (task);
	priv = ssc_client_get_instance_private (client);

	priv->bus = qrtr_bus_new_finish (res, &error);
	if (error) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	node = qrtr_bus_peek_node (priv->bus, priv->node_id);
	if (!node) {
		/*g_task_return_new_error (task,
					 LIBSSC_ERROR,
					 LIBSSC_ERROR_QRTR,
					 "node with id %" G_GUINT32_FORMAT "not found in QRTR bus",
					 priv->node_id);*/
		g_clear_object (&task);
		return;
	}

	g_debug("QRTR node ready");

	/* QRTR node ready, create QMI device */
	qmi_device_new_from_node (node,
		NULL,
		(GAsyncReadyCallback)device_new_ready,
		task);
}


static void
ssc_client_init (SSCClient *self)
{
}

static void
ssc_client_dispose (GObject *object)
{
	G_OBJECT_CLASS (ssc_client_parent_class)->dispose (object);
}

static void
initable_init_async (GAsyncInitable *initable, int io_priority, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
#if QMI_QRTR_SUPPORTED
	{
		GTask *task = NULL;
		GFile *file = NULL;
		SSCClient *self = NULL;
		SSCClientPrivate *priv = NULL;
		g_autofree gchar *id = NULL;

		self = SSC_CLIENT (initable);
		priv = ssc_client_get_instance_private (self);
		priv->sensors = g_array_sized_new (FALSE, FALSE, sizeof (SSCSensor), SSC_NUMBER_OF_SENSORS);

		/* Retrieve QRTR node path */
		g_object_get (self,
			SSC_CLIENT_FILE_PATH, &file,
			NULL);

		task = g_task_new (self, cancellable, callback, user_data);
		g_task_set_task_data (task, self, NULL);

		/* Open node on QRTR bus */
		id = g_file_get_uri (file);
		if (qrtr_get_node_for_uri (id, &priv->node_id)) {
			g_debug("Opening node ID %" G_GUINT32_FORMAT " on QRTR bus", priv->node_id);
			qrtr_bus_new (1000, /* ms */
				NULL,
				(GAsyncReadyCallback)bus_new_ready,
				task);

			return;
		}

		/*g_task_return_new_error (task,
					 LIBSSC_ERROR,
					 LIBSSC_ERROR_QRTR,
					 "Device URI is not a QRTR node: %s",
					 id);*/
		g_clear_object (&task);
		return;
	}
# else
	/*g_task_return_new_error (task,
				 LIBSSC_ERROR,
				 LIBSSC_ERROR_QRTR,
				 "Only QRTR QMI devices are supported. Compile libqmi with QRTR support")*/
	g_clear_object (&task);
	return;
#endif
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
set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SSCClient *self = SSC_CLIENT (object);
	self->priv = ssc_client_get_instance_private (self);

	switch (prop_id) {
		case PROP_FILE:
			self->priv->file = g_value_dup_object (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SSCClient *self = SSC_CLIENT (object);
	self->priv = ssc_client_get_instance_private (self);

	switch (prop_id) {
		case PROP_FILE:
			g_value_set_object (value, self->priv->file);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ssc_client_class_init (SSCClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* Virtual methods */
	object_class->dispose = ssc_client_dispose;
	object_class->set_property = set_property;
	object_class->get_property = get_property;

	/* Signals */
	signals[SIGNAL_REPORT] = g_signal_new ("report",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	/* Properties */
	properties[PROP_FILE] = g_param_spec_object (SSC_CLIENT_FILE_PATH,
			"Device file",
			"File to the underlying device",
			G_TYPE_FILE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_FILE, properties[PROP_FILE]);
}

SSCClient *
ssc_client_new_finish (GAsyncResult *result, GError **error)
{
	GObject *client;
	GObject *source;

	source = g_async_result_get_source_object (result);
	client = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);
	g_object_unref (source);

	if (!client)
		return NULL;

	return SSC_CLIENT (client);
}

void
ssc_client_new (GFile *file, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_return_if_fail (G_IS_FILE (file));

	g_async_initable_new_async (
		SSC_TYPE_CLIENT,
		G_PRIORITY_DEFAULT,
		cancellable,
		callback,
		user_data,
		SSC_CLIENT_FILE_PATH, file,
		NULL);
}
