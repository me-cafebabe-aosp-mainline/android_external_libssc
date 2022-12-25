/*
 * ssc-sensor-proxy: a drop-in replacement for iio-sensor-proxy for SSC support
 * Copyright (C) 2022 Dylan Van Assche
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

#include "proxy-sensor-client.h"
#include "ssc-sensor-proxy.h"

#define UNKNOWN_VALUE 1
#define SENSOR_DISCOVERY_LEN 41

typedef struct {
	QmiDevice *device;
	QmiClientSsc *client;
	QrtrBus *bus;
	guint32 node_id;
	guint indication_report_small_id; // TODO: context_free disconnect
	guint indication_report_large_id;
} Context;

guint8 protobuf_sensor_discovery[SENSOR_DISCOVERY_LEN] = {
	0x0a, 0x12, 0x09, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
       	0xab, 0x11, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
       	0x15, 0x00, 0x02, 0x00, 0x00, 0x1a, 0x04, 0x08, 0x01, 0x10,
       	0x00, 0x22, 0x08, 0x12, 0x06, 0x0a, 0x00, 0x10, 0x01, 0x18,
       	0x00
};

static void
handle_report(guint64 client_id, GArray *protobuf_data)
{
	g_debug("report: client ID: %" G_GUINT64_FORMAT "", client_id);
	g_debug("report: ProtoBuf data:");
	for (guint i = 0; i < protobuf_data->len; i++) {
		guint8 value = g_array_index (protobuf_data, guint8, i);
		g_print ("\\x%02x", value);
	}
	g_printf("\n");
}

static void
report_large_received (QmiClientSsc *client,
	QmiIndicationSscReportLargeOutput *output,
	gpointer user_data)
{
	guint64 client_id;
	GArray *protobuf_data = NULL;
	g_autoptr (GError) error = NULL;

	if (!qmi_indication_ssc_report_large_output_get_client_id (output, &client_id, &error)) {
		g_printerr ("error: cannot extract client ID (large): %s\n", error->message);
		return;
	}

	if (!qmi_indication_ssc_report_large_output_get_protobuf_data (output, &protobuf_data, &error)) {
		g_printerr ("error: cannot extract ProtoBuf data (large): %s\n", error->message);
		return;
	}

	handle_report(client_id, protobuf_data);
}

static void
report_small_received (QmiClientSsc *client,
	QmiIndicationSscReportSmallOutput *output,
	gpointer user_data)
{
	guint64 client_id;
	GArray *protobuf_data = NULL;
	g_autoptr (GError) error = NULL;

	if (!qmi_indication_ssc_report_small_output_get_client_id (output, &client_id, &error)) {
		g_printerr ("error: cannot extract client ID (small): %s\n", error->message);
		return;
	}

	if (!qmi_indication_ssc_report_small_output_get_protobuf_data (output, &protobuf_data, &error)) {
		g_printerr ("error: cannot extract ProtoBuf data (small): %s\n", error->message);
		return;
	}

	handle_report(client_id, protobuf_data);
}

static void
get_sensor_list_ready (QmiClientSsc *client,
	GAsyncResult *res,
	gpointer user_data)
{
	Context *ctx = user_data;
	QmiMessageSscControlOutput *output = NULL;
	g_autoptr (GError) error = NULL;

	output = qmi_client_ssc_control_finish (client, res, &error);
	if (!output) {
		g_printerr ("error: operation failed: %s\n", error->message);
		qmi_message_ssc_control_output_unref (output);
		return;
	}

	if (!qmi_message_ssc_control_output_get_result (output, &error)) {
		g_printerr ("error: couldn't get sensor list: %s\n", error->message);
		qmi_message_ssc_control_output_unref (output);
		return;
	}

	g_debug("Sensor list retrieval success");
	qmi_message_ssc_control_output_unref (output);

	ctx->indication_report_small_id = g_signal_connect (ctx->client,
		"report-small",
		G_CALLBACK (report_small_received),
		ctx);
	ctx->indication_report_large_id = g_signal_connect (ctx->client,
		"report-large",
		G_CALLBACK (report_large_received),
		ctx);
	g_debug("Signals registered for indications");
}

static void
get_sensor_list (Context *ctx) {
	QmiMessageSscControlInput *input = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr (GArray) protobuf = NULL;

	input = qmi_message_ssc_control_input_new ();
	if (!qmi_message_ssc_control_input_set_unknown_value (input, UNKNOWN_VALUE, &error)) {
		g_printerr ("error: inserting Unknown Value failed: %s\n", error->message);
		qmi_message_ssc_control_input_unref (input);
		return;
	}

	/* Build protobuf */
	protobuf = g_array_new(FALSE, FALSE, sizeof (guint8));
	for (guint i=0; i < SENSOR_DISCOVERY_LEN; i++) {
		g_array_append_val (protobuf, protobuf_sensor_discovery[i]);
	}

	if (!qmi_message_ssc_control_input_set_protobuf_data (input, protobuf, &error)) {
		g_printerr ("error: inserting ProtoBuf data failed: %s\n", error->message);
		qmi_message_ssc_control_input_unref (input);
		return;
	}

	qmi_client_ssc_control (ctx->client,
		input,
		10,
		NULL,
		(GAsyncReadyCallback)get_sensor_list_ready,
		ctx);
	qmi_message_ssc_control_input_unref (input);
}

static void
allocate_client_ready (QmiDevice *device, GAsyncResult *result, gpointer user_data) {
	g_autoptr(GError) error = NULL;
	Context *ctx = user_data;

	ctx->client = QMI_CLIENT_SSC (qmi_device_allocate_client_finish (device, result, &error));

	if (!ctx->client) {
		g_printerr ("error: couldn't create client for SSC service: %s\n",
			error->message);
		exit(EXIT_FAILURE);
	}

	/* QMI client allocated */
	get_sensor_list(ctx);
}

static void
device_open_ready (QmiDevice *device,
	GAsyncResult *result,
	gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	Context *ctx = user_data;

	if (!qmi_device_open_finish (device, result, &error)) {
		g_printerr ("error: couldn't open the QMI device: %s\n", error->message);
		exit (EXIT_FAILURE);
	}

	g_debug ("QMI device at '%s' ready", qmi_device_get_path_display (device));

	/* QMI device opened, allocate client */
	qmi_device_allocate_client (device,
		QMI_SERVICE_SSC,
		QMI_CID_NONE,
		10,
		NULL,
		(GAsyncReadyCallback)allocate_client_ready,
		ctx);
}

static void
device_new_ready (GObject *unused,
	GAsyncResult *res,
	gpointer user_data)
{
	QmiDeviceOpenFlags open_flags = QMI_DEVICE_OPEN_FLAGS_NONE;
	g_autoptr(GError) error = NULL;
	Context *ctx = user_data;

	ctx->device = qmi_device_new_finish (res, &error);
	if (!ctx->device) {
		g_printerr ("error: couldn't create QMI device: %s\n", error->message);
		exit (EXIT_FAILURE);
	}

	/* Indications are expected as they report all sensor data values */
	open_flags |= QMI_DEVICE_OPEN_FLAGS_AUTO;
	open_flags |= QMI_DEVICE_OPEN_FLAGS_EXPECT_INDICATIONS;

	/* QMI device created, open device */
	qmi_device_open (ctx->device,
		open_flags,
		15,
		NULL,
		(GAsyncReadyCallback)device_open_ready,
		ctx);
}

static void
bus_new_ready (GObject *source,
	GAsyncResult *res,
	gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	QrtrNode *node;
	Context *ctx = user_data;

	ctx->bus = qrtr_bus_new_finish (res, &error);
	if (!ctx->bus) {
		g_printerr ("error: couldn't access QRTR bus: %s\n", error->message);
		exit (EXIT_FAILURE);
	}

	node = qrtr_bus_peek_node (ctx->bus, ctx->node_id);
	if (!node) {
		g_printerr ("error: node with id %" G_GUINT32_FORMAT "not found in QRTR bus\n", ctx->node_id);
		exit (EXIT_FAILURE);
	}

	/* QRTR node ready, create QMI device */
	qmi_device_new_from_node (node,
		NULL,
		(GAsyncReadyCallback)device_new_ready,
		ctx);
}

gboolean
sensor_client_init(GFile *file) {
#if QMI_QRTR_SUPPORTED
	{
		g_autofree gchar *id = NULL;
		Context *ctx = NULL;

		/* Initialize context */
		ctx = g_slice_new (Context);

		/* Open node on QRTR bus */
		id = g_file_get_uri (file);
		if (qrtr_get_node_for_uri (id, &ctx->node_id)) {
			g_debug("Opening node ID %" G_GUINT32_FORMAT " on QRTR bus", ctx->node_id);
			qrtr_bus_new (1000, /* ms */
				NULL,
				(GAsyncReadyCallback)bus_new_ready,
				ctx);
			return TRUE;
		}

		g_printerr ("error: Device URI is not a QRTR node: %s\n", id);
		return FALSE;
	}
# else
	g_printerr("error: Only QRTR QMI devices are supported. Compile libqmi with QRTR support.\n");
	return FALSE;
#endif
}
