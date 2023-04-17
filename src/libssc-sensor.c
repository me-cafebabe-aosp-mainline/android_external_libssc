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
	PROP_NAME = 1,
	PROP_UID_LOW = 2,
	PROP_UID_HIGH = 3,
	PROP_VENDOR = 4,
	PROP_DATA_TYPE = 5,
	PROP_STREAM_TYPE = 6,
	PROP_AVAILABLE = 7,
	PROP_SAMPLE_RATE = 8,
	PROP_CLIENT = 9,
	PROP_FILE = 10,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES];

typedef struct {
	GTask *task;
	SSCSensor *sensor;
} ReportReceivedContext;

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
	GFile *file;
} SSCSensorPrivate;

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (SSCSensor, ssc_sensor, G_TYPE_OBJECT,
			 G_ADD_PRIVATE (SSCSensor)
			 G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

static void
attribute (SSCSensor *self, GTask *task);

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

	if (!ssc_client_send_finish (self, result, &error)) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		g_warning ("Sensor disable request failed: %s", error->message);
		return;
	}

	g_debug ("Sensor disable request sent successfully");
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
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

	SSC_SENSOR_GET_CLASS (self)->close (self, cancellable, callback, user_data);
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

	if (!ssc_client_send_finish (self, result, &error)) {
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
	g_autoptr (GArray) buf = NULL;
	ReportReceivedContext *ctx;
	guint32 msg_id;

	priv = ssc_sensor_get_instance_private (self);
	task = g_task_new (self, cancellable, callback, user_data);

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
		
		msg_id = SSC_MSG_REQUEST_ENABLE_REPORT_CONTINUOUS;
	/* 
	 * Sensors which support on-change do not need any configuration,
	 * only a different message ID to enable them.
	 */
	} else if (priv->stream_type == SSC_STREAM_TYPE_ON_CHANGE) {
		msg_id = SSC_MSG_REQUEST_ENABLE_REPORT_ON_CHANGE;
	} else
		g_assert_not_reached ();

	ctx = g_slice_new (ReportReceivedContext);
	ctx->task = task;
	ctx->sensor = self;

	/* Start listening for report signals */
	priv->report_id = g_signal_connect (priv->client,
			"report",
			G_CALLBACK (report_received),
			ctx);

	ssc_client_send (priv->client,
			 priv->uid_high,
			 priv->uid_low,
			 msg_id,
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

	SSC_SENSOR_GET_CLASS (self)->open (self, cancellable, callback, user_data);
}

/*****************************************************************************/

static void
report_received (SSCClient *self, guint32 msg_id, guint64 uid_high, guint64 uid_low, GArray *buf, gpointer user_data)
{
	SscSuidResponse *suid_msg = NULL;
	SscAttrResponse *attr_msg = NULL;
	SscConfigResponse *config_msg = NULL;
	SSCSensorPrivate *priv = NULL;
	ReportReceivedContext *ctx = user_data;
	gboolean attributes_populated = FALSE;

	priv = ssc_sensor_get_instance_private (ctx->sensor);

	/* Discover response */
	if (uid_high == SSC_SENSOR_UID_SUID_HIGH && uid_low == SSC_SENSOR_UID_SUID_LOW && msg_id == SSC_MSG_RESPONSE_SUID) {
		suid_msg = ssc_suid_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

		/* Ignore if data type does not match due to concurrency */
		if (g_strcmp0 (suid_msg->data_type, priv->data_type) != 0) {
			ssc_suid_response__free_unpacked (suid_msg, NULL);
			return;
		}

		/* Only default sensor for data type is reported */
		if (suid_msg != NULL && suid_msg->n_uid > 0) {
			priv->uid_high = suid_msg->uid[0]->high;
			priv->uid_low = suid_msg->uid[0]->low;

			g_debug ("Discovered '%s' sensor (%016lX %016lX)", priv->data_type, priv->uid_high, priv->uid_low);
			ssc_suid_response__free_unpacked (suid_msg, NULL);

			/* Sensor discovered, populate attributes */
			attribute (ctx->sensor, ctx->task);
			return;
		/* No sensor available for specified data type, complete task */
		} else {
			g_debug ("No '%s' sensor available", priv->data_type);
			ssc_suid_response__free_unpacked (suid_msg, NULL);
			
			g_task_return_boolean (ctx->task, FALSE);
			g_clear_object (&ctx->task);
			g_slice_free (ReportReceivedContext, ctx);
			return;
		}
	/* Attributes populating response */
	} else if (uid_high == priv->uid_high && uid_low == priv->uid_low && msg_id == SSC_MSG_RESPONSE_GET_ATTRIBUTES) {
		attr_msg = ssc_attr_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);

		if (attr_msg != NULL) {
			for (gsize i = 0; i < attr_msg->n_attr; i++) {
				switch (attr_msg->attr[i]->id) {
					case SSC_ATTRIBUTE_NAME:
						if (attr_msg->attr[i]->value_array->n_v == 1 && attr_msg->attr[i]->value_array->v[0]->s)
							priv->name = g_strdup (attr_msg->attr[i]->value_array->v[0]->s);
						break;
					case SSC_ATTRIBUTE_VENDOR:
						if (attr_msg->attr[i]->value_array->n_v == 1 && attr_msg->attr[i]->value_array->v[0]->s)
							priv->vendor = g_strdup (attr_msg->attr[i]->value_array->v[0]->s);
						break;
					case SSC_ATTRIBUTE_AVAILABLE:
						if (attr_msg->attr[i]->value_array->n_v == 1 && attr_msg->attr[i]->value_array->v[0]->has_b)
							priv->available = attr_msg->attr[i]->value_array->v[0]->b;
						break;
					case SSC_ATTRIBUTE_SAMPLE_RATE:
						/* Only a single sample rate is supported for now. */
						if (attr_msg->attr[i]->value_array->n_v >= 1 && attr_msg->attr[i]->value_array->v[0]->has_f)
							priv->sample_rate = attr_msg->attr[i]->value_array->v[0]->f;
						break;
					case SSC_ATTRIBUTE_STREAM_TYPE:
						if (attr_msg->attr[i]->value_array->n_v == 1 && attr_msg->attr[i]->value_array->v[0]->has_i)
							priv->stream_type = attr_msg->attr[i]->value_array->v[0]->i;
						break;
				}
			}

			attributes_populated = TRUE;
			g_debug ("Attributes populated for '%s' sensor (%016lX %016lX)", priv->data_type, priv->uid_high, priv->uid_low);
			g_debug ("  name: %s", priv->name);
			g_debug ("  vendor: %s", priv->vendor);
			g_debug ("  data-type: %s", priv->data_type);
			g_debug ("  stream-type: %s", priv->stream_type == SSC_STREAM_TYPE_CONTINUOUS ? "continuous" : "on-change");
			g_debug ("  sample-rate: %f Hz", priv->sample_rate);
			g_debug ("  available: %s", priv->available ? "yes" : "no");
		}

		/* Sensor initialized, complete task and stop listening */
		if (ctx->task) {
			g_signal_handler_disconnect (self, priv->report_id);
			priv->report_id = 0;
			g_task_return_boolean (ctx->task, attributes_populated);
			g_clear_object (&ctx->task);
			g_slice_free (ReportReceivedContext, ctx);
		}
		
		ssc_attr_response__free_unpacked (attr_msg, NULL);
		return;
	} else if (uid_high == priv->uid_high && uid_low == priv->uid_low && msg_id == SSC_MSG_RESPONSE_ENABLE_REPORT) {
		config_msg = ssc_config_response__unpack (NULL, buf->len, (const uint8_t *) buf->data);
		
		g_debug ("Configuration updated for '%s' sensor (%016lX %016lX)", priv->data_type, priv->uid_high, priv->uid_low);
		g_debug ("  mode: %s", config_msg->mode ? config_msg->mode : "UNKNOWN");
		g_debug ("  sample-rate: %f Hz", config_msg->has_sample_rate ? config_msg->sample_rate : 0.0);

		/* Configuration updated, complete task and stop listening */
		if (ctx->task) {
			g_signal_handler_disconnect (self, priv->report_id);
			priv->report_id = 0;
			g_task_return_boolean (ctx->task, TRUE);
			g_clear_object (&ctx->task);	
			g_slice_free (ReportReceivedContext, ctx);
		}

		ssc_config_response__free_unpacked (config_msg, NULL);
		return;
	}
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
attribute (SSCSensor *self, GTask *task)
{
	g_autoptr (GArray) buf = NULL;
	SscAttrRequest msg;
	SSCSensorPrivate *priv = NULL;

	priv = ssc_sensor_get_instance_private (self);

	/* Build attributes request */
	ssc_attr_request__init (&msg);
	msg.has_enable_updates = true;
	msg.enable_updates = false;
	buf = g_array_new (FALSE, FALSE, 1);
	g_array_set_size (buf, ssc_attr_request__get_packed_size (&msg));
	ssc_attr_request__pack (&msg, (unsigned char*) buf->data);

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

/*****************************************************************************/

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
discover (SSCSensor *self, GTask *task)
{
	SSCSensorPrivate *priv = NULL;
	SscSuidRequest msg;
	g_autoptr (GArray) buf = NULL;

	priv = ssc_sensor_get_instance_private (self);

	g_debug ("Discovering sensor UID for data type '%s'", priv->data_type);

	/*
	 * Request for sensors for given datatype, if multiple sensors support a datatype,
	 * only return the default sensor. Do not monitor for hotplugged sensors.
	 */
	ssc_suid_request__init (&msg);
	msg.data_type = priv->data_type;
	msg.has_enable_updates = true;
	msg.enable_updates = false;
	msg.has_only_default_values = true;
	msg.only_default_values = true;

	buf = g_array_new (FALSE, FALSE, 1);
	g_array_set_size (buf, ssc_suid_request__get_packed_size (&msg));
	ssc_suid_request__pack (&msg, (unsigned char*) buf->data);

	ssc_client_send (priv->client,
			 SSC_SENSOR_UID_SUID_HIGH,
			 SSC_SENSOR_UID_SUID_LOW,
			 SSC_MSG_REQUEST_SUID,
			 buf,
			 NULL,
			 (GAsyncReadyCallback)discovery_ready,
			 NULL); 
}

/*****************************************************************************/

static void
client_ready (SSCClient *client, GAsyncResult *result, gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	ReportReceivedContext *ctx = NULL;
	SSCSensorPrivate *priv = NULL;
	g_autoptr (GError) error = NULL;
	SSCSensor *self = NULL;

	self = g_task_get_source_object (task);
	priv = ssc_sensor_get_instance_private (self);

	/* Client allocation */
	priv->client = ssc_client_new_finish (result, &error);
	if (!priv->client) {
		g_task_return_boolean (task, FALSE);
		g_object_unref (task);
		return;
	}

	ctx = g_slice_new (ReportReceivedContext);
	ctx->task = task;
	ctx->sensor = self;

	/* Start listening for report signals */
	priv->report_id = g_signal_connect (priv->client,
			"report",
			G_CALLBACK (report_received),
			ctx);

	discover (self, task);
}

static void
initable_init_async (GAsyncInitable *initable, int io_priority, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	SSCSensor *self = NULL;
	g_autoptr (GFile) file = NULL;

	self = SSC_SENSOR (initable);
	task = g_task_new (self, cancellable, callback, user_data);

	g_object_get (self,
		      SSC_CLIENT_FILE_PATH, &file,
		      NULL);
	ssc_client_new (file, NULL, (GAsyncReadyCallback)client_ready, task);
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
		case PROP_FILE:
			priv->file = g_value_dup_object (value);
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
		case PROP_FILE:
			g_value_set_object (value, priv->file);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
sensor_dispose (GObject *object)
{
	SSCSensor *self = SSC_SENSOR (object);
	SSCSensorPrivate *priv = ssc_sensor_get_instance_private (self);

	g_free (&priv->name);
	g_free (&priv->vendor);
	g_free (&priv->data_type);
	g_clear_object (&priv->client);
	g_clear_object (&priv->file);
}

static void
ssc_sensor_class_init (SSCSensorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	SSCSensorClass *ssc_sensor_class = SSC_SENSOR_CLASS (klass);

	object_class->get_property = sensor_get_property;
	object_class->set_property = sensor_set_property;
	object_class->dispose = sensor_dispose;

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

	properties[PROP_FILE] =
		g_param_spec_object (SSC_CLIENT_FILE_PATH,
				     "Device file",
				     "File to the underlying device",
				     G_TYPE_FILE,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_FILE, properties[PROP_FILE]);
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
ssc_sensor_new (GFile *file, gchar *data_type, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	g_async_initable_new_async (
			SSC_TYPE_SENSOR,
			G_PRIORITY_DEFAULT,
			cancellable,
			callback,
			user_data,
			SSC_SENSOR_DATA_TYPE, data_type,
			SSC_CLIENT_FILE_PATH, file,
			NULL);
}
