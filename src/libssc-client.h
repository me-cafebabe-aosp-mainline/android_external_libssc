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

#ifndef _LIBSSC_CLIENT_H_
#define _LIBSSC_CLIENT_H_

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gio/gio.h>
#include <libqmi-glib.h>

typedef enum {
	LIBSSC_ERROR_QRTR_DEVICE_URI,
	LIBSSC_ERROR_QRTR_UNSUPPORTED,
	LIBSSC_ERROR_QRTR_NODE_NOT_FOUND,
} SSCError;
G_DEFINE_QUARK(ssc-error-quark, ssc_error)
#define LIBSSC_ERROR (ssc_error_quark())

typedef struct {
	QmiDevice *device;
	QmiClientSsc *qmi_client_ssc;
	QrtrBus *bus;
	guint32 node_id;
	guint indication_report_small_id;
	guint indication_report_large_id;
} SSCClient;

void
ssc_client_init (GObject *self, GFile *file, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data);

SSCClient *
ssc_client_init_finish (GObject *self, GAsyncResult *res, GError **error);

SSCClient *
ssc_client_init_sync (GObject *self, GFile *file, GError **error);

#endif /* _LIBSSC_CLIENT_H_ */
