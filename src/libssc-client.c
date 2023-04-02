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

static void
allocate_client_ready (QmiDevice *device, GAsyncResult *result, gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	GTask *task = NULL;
	SSCClient *client = NULL;

	task = G_TASK (user_data);
	client = g_task_get_task_data (task);

	client->qmi_client_ssc = QMI_CLIENT_SSC (qmi_device_allocate_client_finish (device, result, &error));

	if (error) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	/* QMI client allocated */
	g_debug ("QMI SSC client allocated");
	g_task_return_pointer (task, client, NULL); // TODO: destroy
	g_clear_object (&task);
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
device_new_ready (GObject *unused, GAsyncResult *res, gpointer user_data)
{
	QmiDeviceOpenFlags open_flags = QMI_DEVICE_OPEN_FLAGS_NONE;
	g_autoptr(GError) error = NULL;
	GTask *task = NULL;
	SSCClient *client = NULL;

	task = G_TASK (user_data);
	client = g_task_get_task_data (task);

	client->device = qmi_device_new_finish (res, &error);
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
	qmi_device_open (client->device,
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

	task = G_TASK (user_data);
	client = g_task_get_task_data (task);

	client->bus = qrtr_bus_new_finish (res, &error);
	if (error) {
		g_task_return_error (task, error);
		g_clear_object (&task);
		return;
	}

	node = qrtr_bus_peek_node (client->bus, client->node_id);
	if (!node) {
	/*	g_task_return_new_error (task,
					 LIBSSC_ERROR,
					 LIBSSC_ERROR_QRTR,
					 "node with id %" G_GUINT32_FORMAT "not found in QRTR bus",
					 client->node_id);*/
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
client_init (GTask *task)
{
#if QMI_QRTR_SUPPORTED
	{
		GFile *file = NULL;
		SSCClient *client = NULL;
		g_autofree gchar *id = NULL;

		file = g_task_get_task_data (task);

		client = g_slice_new (SSCClient);
		g_task_set_task_data (task, client, NULL);

		/* Open node on QRTR bus */
		id = g_file_get_uri (file);
		if (qrtr_get_node_for_uri (id, &client->node_id)) {
			g_debug("Opening node ID %" G_GUINT32_FORMAT " on QRTR bus", client->node_id);
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

static void client_free (SSCClient *client)
{
	// TODO: free
	return;
}

static void
client_init_thread(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	g_task_set_check_cancellable (task, FALSE);

	client_init (task);
}

void
ssc_client_init (GObject *self, GFile *file, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = NULL;
	
	task = g_task_new (file, cancellable, callback, user_data);
	g_task_set_task_data (task, file, NULL);
	g_task_set_check_cancellable (task, FALSE);

	client_init (task);
}

SSCClient *
ssc_client_init_finish (GObject *self, GAsyncResult *res, GError **error)
{
	return g_task_propagate_pointer (G_TASK (res), error);
}

SSCClient *
ssc_client_init_sync (GObject *self, GFile *file, GError **error)
{
	GTask *task = NULL;
	SSCClient *client = NULL;
	
	task = g_task_new (self, NULL, NULL, NULL);
	g_task_set_task_data (task, file, NULL);
	g_task_run_in_thread_sync (task, client_init_thread);

	client = g_task_propagate_pointer (task, error);
	g_object_unref (task);
	return client;
}
