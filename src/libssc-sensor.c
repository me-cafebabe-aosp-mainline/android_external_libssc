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

#include "libssc-sensor.h"

enum {
	SIGNAL_MEASUREMENT_ACCELEROMETER,
	SIGNAL_MEASUREMENT_MAGNETOMETER,
	SIGNAL_MEASUREMENT_PROXIMITY,
	SIGNAL_MEASUREMENT_LIGHT,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

enum {
	PROP_NAME = 1,
	PROP_UID_LOW = 2,
	PROP_UID_HIGH = 3,
	PROP_VENDOR = 4,
	PROP_DATA_TYPE = 5,
	PROP_STREAM_TYPE = 6,
	PROP_AVAILABLE = 7,
	PROP_SAMPLE_RATE = 8,
	PROP_CLIENT = 9,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES];

typedef struct {
	GTask *task;
	SSCSensor *sensor;
} ReportReceivedContext;

static void async_initable_iface_init (GAsyncInitableIface *iface);

typedef struct _SSCSensorPrivate {
	guint64 uid_low;
	guint64 uid_high;
	gchar *name;
	gchar *vendor;
	gchar *data_type;
	guint stream_type;
	gboolean available;
	gfloat sample_rate;

	SSCClient *client;
	guint report_id;
	gboolean attr_populated;
} SSCSensorPrivate;

G_DEFINE_TYPE_WITH_CODE (SSCSensor, ssc_sensor, G_TYPE_OBJECT,
			 G_ADD_PRIVATE (SSCSensor)
			 G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data);

/*****************************************************************************/

static gboolean
sensor_close_finish (SSCSensor *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
sensor_close_ready (SSCClient *self, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (ssc_client_send_finish (self, result, &error)) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		g_warning ("Sensor disable request failed: %s", error->message);
		return;
	}

	g_debug ("Sensor disable request sent successfully");
}

static void
sensor_close (SSCSensor *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensorPrivate *priv = NULL;

	task = g_task_new (self, cancellable, callback, user_data);
	priv = ssc_sensor_get_instance_private (self);

	g_info ("Disabling sensor (%016lX %016lX)", priv->uid_high, priv->uid_low);

	ssc_client_send (priv->client,
			 priv->uid_high,
			 priv->uid_low,
			 SSC_MSG_REQUEST_DISABLE_REPORT,
			 NULL,
			 NULL,
			 (GAsyncReadyCallback)sensor_close_ready,
			 task);
}

gboolean
ssc_sensor_close_finish (SSCSensor *self, GAsyncResult *result, GError **error)
{
	return SSC_SENSOR_GET_CLASS (self)->close_finish (self, result, error);
}

void
ssc_sensor_close (SSCSensor *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_assert (SSC_SENSOR_GET_CLASS (self)->close &&
		  SSC_SENSOR_GET_CLASS (self)->close_finish);

	return SSC_SENSOR_GET_CLASS (self)->close (self, cancellable, callback, user_data);
}

/*****************************************************************************/

static gboolean
sensor_open_finish (SSCSensor *self, GAsyncResult *result, GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
sensor_open_ready (SSCClient *self, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	g_autoptr (GError) error = NULL;

	if (ssc_client_send_finish (self, result, &error)) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		g_warning ("Sensor enable request failed: %s", error->message);
		return;
	}

	g_debug ("Sensor enable request sent successfully");
}

static void 
sensor_open (SSCSensor *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	SSCSensorPrivate *priv = NULL;
	GTask *task = NULL;
	SscEnableConfigRequest msg;
	GArray *buf = NULL;
	ReportReceivedContext *ctx;

	priv = ssc_sensor_get_instance_private (self);
	task = g_task_new (self, cancellable, callback, user_data);

	ctx = g_slice_new (ReportReceivedContext);
	ctx->task = task;
	ctx->sensor = self;

	if (!priv->available) {
		g_warning ("Cannot open sensor, unavailable");
		//g_task_return_error ();
		g_object_unref (task);
		return;
	}

	g_info ("Enabling sensor (%016lX %016lX) in '%s' mode", priv->uid_high, priv->uid_low, priv->stream_type == SSC_STREAM_TYPE_CONTINUOUS? "continuous" : "on-change");

	/*
	 * Sensors which support continuous streaming need a sample rate,
	 * on-change sensors do not have a message payload
	 */
	if (priv->stream_type == SSC_STREAM_TYPE_CONTINUOUS) {
		ssc_enable_config_request__init (&msg);

		if (priv->sample_rate <= 0.0) {
			g_warning ("Sample rate unavailable");
			//g_task_return_error ();
			g_object_unref (task);
			return;
		}
		
		msg.sample_rate = priv->sample_rate;
		buf = g_array_new (FALSE, FALSE, 1);
		g_array_set_size (buf, ssc_enable_config_request__get_packed_size (&msg));
		ssc_enable_config_request__pack (&msg, (unsigned char*) buf->data);
	}

	/* Start listening for report signals */
	priv->report_id = g_signal_connect (priv->client,
			"report",
			G_CALLBACK (report_received),
			ctx);

	ssc_client_send (priv->client,
			 priv->uid_high,
			 priv->uid_low,
			 SSC_MSG_REQUEST_ENABLE_REPORT_ON_CHANGE,
			 buf,
			 NULL,
			 (GAsyncReadyCallback)sensor_open_ready,
			 task);
}

gboolean
ssc_sensor_open_finish (SSCSensor *self, GAsyncResult *result, GError **error)
{
	return SSC_SENSOR_GET_CLASS (self)->open_finish (self, result, error);
}

void
ssc_sensor_open (SSCSensor *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_assert (SSC_SENSOR_GET_CLASS (self)->open &&
		  SSC_SENSOR_GET_CLASS (self)->open_finish);

	return SSC_SENSOR_GET_CLASS (self)->open (self, cancellable, callback, user_data);
}

/*****************************************************************************/

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SscAttrResponse *msg = NULL;
	SSCSensorPrivate *priv = NULL;
	ReportReceivedContext *ctx = user_data;
	gboolean attributes_populated = FALSE;

	priv = ssc_sensor_get_instance_private (ctx->sensor);

	/* Only handle reports with matching UID */
	if (priv->uid_high != uid_high || priv->uid_low != uid_low)
		return;

	/* Attributes populating response */
	if (msg_id == SSC_MSG_RESPONSE_GET_ATTRIBUTES) {
		msg = ssc_attr_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

		if (msg != NULL) {
			for (gsize i = 0; i < msg->n_attr; i++) {
				switch (msg->attr[i]->id) {
					case SSC_ATTRIBUTE_NAME:
						if (msg->attr[i]->value_array->n_v == 1 && msg->attr[i]->value_array->v[0]->s)
							priv->name = g_strdup (msg->attr[i]->value_array->v[0]->s);
						break;
					case SSC_ATTRIBUTE_VENDOR:
						if (msg->attr[i]->value_array->n_v == 1 && msg->attr[i]->value_array->v[0]->s)
							priv->vendor = g_strdup (msg->attr[i]->value_array->v[0]->s);
						break;
					case SSC_ATTRIBUTE_AVAILABLE:
						if (msg->attr[i]->value_array->n_v == 1 && msg->attr[i]->value_array->v[0]->has_b)
							priv->available = msg->attr[i]->value_array->v[0]->b;
						break;
					case SSC_ATTRIBUTE_SAMPLE_RATE:
						/* Only a single sample rate is supported for now. */
						if (msg->attr[i]->value_array->n_v >= 1 && msg->attr[i]->value_array->v[0]->has_f)
							priv->sample_rate = msg->attr[i]->value_array->v[0]->f;
						break;
					case SSC_ATTRIBUTE_STREAM_TYPE:
						if (msg->attr[i]->value_array->n_v == 1 && msg->attr[i]->value_array->v[0]->has_i)
							priv->stream_type = msg->attr[i]->value_array->v[0]->i;
						break;
				}
			}

			ssc_attr_response__free_unpacked (msg, NULL);
			attributes_populated = TRUE;
		} else
			g_warning ("Cannot unpack attribute message response");

		/* Sensor initialized, complete task. Only perform this step during initialization not when handling measurement reports */
		if (ctx->task != NULL) {
			if (attributes_populated) {
				g_info ("Initialized '%s' sensor (%016lX %016lX)", priv->data_type, priv->uid_high, priv->uid_low);
				g_info ("  name: %s", priv->name);
				g_info ("  vendor: %s", priv->vendor);
				g_info ("  data-type: %s", priv->data_type);
				g_info ("  stream-type: %s", priv->stream_type == SSC_STREAM_TYPE_CONTINUOUS ? "continuous" : "on-change");
				g_info ("  sample-rate: %f Hz", priv->sample_rate);
				g_info ("  available: %s", priv->available ? "yes" : "no");
			} else
				g_info ("No '%s' sensor available", priv->data_type);

			/* Task is done, stop listening */
			g_signal_handler_disconnect (priv->client, priv->report_id);
			priv->report_id = 0;

			g_task_return_boolean (ctx->task, attributes_populated);
			g_clear_object (&ctx->task);

			g_slice_free (ReportReceivedContext, ctx);
		}

		return;
	}
	
	g_warning ("Unsupported message %" G_GUINT32_FORMAT " for '%s' sensor (%016lX %016lX)", msg_id, priv->data_type, priv->uid_high, priv->uid_low);
	g_assert_not_reached ();
}

/*****************************************************************************/

static void
attribute_ready (SSCClient *self, GAsyncResult *result, gpointer user_data)
{
	g_autoptr(GError) error = NULL;

	if (!ssc_client_send_finish (self, result, &error)) {
		g_warning ("Sensor attribute request failed: %s", error->message);
		return;
	}

	g_debug ("Sensor attribute request sent successfully");
}

static void
initable_init_async (GAsyncInitable *initable, int io_priority, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensor *self = NULL;
	SSCSensorPrivate *priv = NULL;
	SscAttrRequest msg;
	GArray *buf = NULL;
	ReportReceivedContext *ctx = NULL;

	self = SSC_SENSOR (initable);
	priv = ssc_sensor_get_instance_private (self);
	task = g_task_new (self, cancellable, callback, user_data);

	ctx = g_slice_new (ReportReceivedContext);
	ctx->task = task;
	ctx->sensor = self;

	/* Build attributes request */
	ssc_attr_request__init (&msg);
	msg.has_enable_updates = true;
	msg.enable_updates = false;
	buf = g_array_new (FALSE, FALSE, 1);
	g_array_set_size (buf, ssc_attr_request__get_packed_size (&msg));
	ssc_attr_request__pack (&msg, (unsigned char*) buf->data);

	/* Start listening for report signals */
	priv->report_id = g_signal_connect (priv->client,
			"report",
			G_CALLBACK (report_received),
			ctx);

	/* Send attribute request */
	ssc_client_send (priv->client,
			 priv->uid_high,
			 priv->uid_low,
			 SSC_MSG_REQUEST_GET_ATTRIBUTES,
			 buf,
			 NULL,
			 (GAsyncReadyCallback)attribute_ready,
			 NULL);
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
sensor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SSCSensor *self = SSC_SENSOR (object);
	SSCSensorPrivate *priv = ssc_sensor_get_instance_private (self);

	switch (prop_id) {
		case PROP_UID_LOW:
			priv->uid_low = g_value_get_uint64 (value);
			break;
		case PROP_UID_HIGH:
			priv->uid_high = g_value_get_uint64 (value);
			break;
		case PROP_DATA_TYPE:
			g_free (priv->data_type);
			priv->data_type = g_value_dup_string (value);
			break;
		case PROP_CLIENT:
			priv->client = g_value_dup_object (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
sensor_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SSCSensor *self = SSC_SENSOR (object);
	SSCSensorPrivate *priv = ssc_sensor_get_instance_private (self);

	switch (prop_id) {
		case PROP_UID_LOW:
			g_value_set_uint64 (value, priv->uid_low);
			break;
		case PROP_UID_HIGH:
			g_value_set_uint64 (value, priv->uid_high);
			break;
		case PROP_NAME:
			g_value_set_string (value, priv->name);
			break;
		case PROP_VENDOR:
			g_value_set_string (value, priv->vendor);
			break;
		case PROP_DATA_TYPE:
			g_value_set_string (value, priv->data_type);
			break;
		case PROP_STREAM_TYPE:
			g_value_set_uint (value, priv->stream_type);
			break;
		case PROP_AVAILABLE:
			g_value_set_boolean (value, priv->available);
			break;
		case PROP_SAMPLE_RATE:
			g_value_set_float (value, priv->sample_rate);
			break;
		case PROP_CLIENT:
			g_value_set_object (value, priv->client);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ssc_sensor_class_init (SSCSensorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	SSCSensorClass *ssc_sensor_class = SSC_SENSOR_CLASS (klass);

	object_class->get_property = sensor_get_property;
	object_class->set_property = sensor_set_property;

	ssc_sensor_class->open = sensor_open;
	ssc_sensor_class->open_finish = sensor_open_finish;
	ssc_sensor_class->close = sensor_close;
	ssc_sensor_class->close_finish = sensor_close_finish;

	properties[PROP_UID_LOW] =
		g_param_spec_uint64 (SSC_SENSOR_UID_LOW,
				     "Sensor UID low",
				     "Lower 64 bits of the sensor UID",
				     0,
				     G_MAXUINT64,
				     0,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_UID_LOW, properties[PROP_UID_LOW]);

	properties[PROP_UID_HIGH] =
		g_param_spec_uint64 (SSC_SENSOR_UID_HIGH,
				     "Sensor UID high",
				     "Higher 64 bits of the sensor UID",
				     0,
				     G_MAXUINT64,
				     0,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_UID_HIGH, properties[PROP_UID_HIGH]);

	properties[PROP_NAME] = 
		g_param_spec_string (SSC_SENSOR_NAME,
				     "Sensor driver name",
				     "Name of the sensor driver.",
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_NAME, properties[PROP_NAME]);

	properties[PROP_VENDOR] = 
		g_param_spec_string (SSC_SENSOR_VENDOR,
				     "Sensor vendor",
				     "Name of the vendor of the sensor.",
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VENDOR, properties[PROP_VENDOR]);

	properties[PROP_DATA_TYPE] = 
		g_param_spec_string (SSC_SENSOR_DATA_TYPE,
				     "Data type",
				     "The data type supported by the sensor.",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_DATA_TYPE, properties[PROP_DATA_TYPE]);

	properties[PROP_STREAM_TYPE] = 
		g_param_spec_string (SSC_SENSOR_STREAM_TYPE,
				     "Stream type",
				     "The stream type supported by the sensor.",
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_STREAM_TYPE, properties[PROP_STREAM_TYPE]);

	properties[PROP_AVAILABLE] = 
		g_param_spec_string (SSC_SENSOR_AVAILABLE,
				     "Availability",
				     "If the sensor is available for measurements.",
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_AVAILABLE, properties[PROP_AVAILABLE]);

	properties[PROP_SAMPLE_RATE] = 
		g_param_spec_string (SSC_SENSOR_SAMPLE_RATE,
				     "Sample rate",
				     "The sample rate in Hz supported by the sensor.",
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SAMPLE_RATE, properties[PROP_SAMPLE_RATE]);

	properties[PROP_CLIENT] =
		g_param_spec_object (SSC_SENSOR_CLIENT,
				     "SSC Client",
				     "Reference to SSC Client",
				     SSC_TYPE_CLIENT,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_CLIENT, properties[PROP_CLIENT]);
}

static void
ssc_sensor_init (SSCSensor *self)
{
}

SSCSensor *
ssc_sensor_new_finish (GAsyncResult *result, GError **error)
{
	GObject *sensor;
	GObject *source;

	source = g_async_result_get_source_object (result);
	sensor = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, error);
	g_object_unref (source);

	if (!sensor)
		return NULL;

	return SSC_SENSOR (sensor);
}

void
ssc_sensor_new (guint64 uid_high, guint64 uid_low, gchar *data_type, SSCClient *client, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	return g_async_initable_new_async (
			SSC_TYPE_SENSOR,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
			SSC_SENSOR_UID_HIGH, uid_high,
			SSC_SENSOR_UID_LOW, uid_low,
			SSC_SENSOR_DATA_TYPE, data_type,
			SSC_SENSOR_CLIENT, client,
			NULL);
}
