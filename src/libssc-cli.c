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

#include "libssc.h"
#include "libssc-client.h"
#include "libssc-version.h"

int main(int argc, char *argv[])
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GError) err = NULL;
	GFile *file = NULL;
	g_autofree gchar *device_str = "qrtr://0";
	struct SSCCli cli;
	gboolean print_version = FALSE;
	gboolean debug = FALSE;
	const GOptionEntry options[] = {
		{ "version", 'v', 0, G_OPTION_ARG_NONE, &print_version, "Print version information and exit.", NULL },
		{ "device", 'v', 0, G_OPTION_ARG_STRING, &device_str, "QMI device to use, default 'qrtr://0'.", NULL },
		{ "debug", 'v', 0, G_OPTION_ARG_NONE, &debug, "Enable debug logs.", NULL },
		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};

	opt_context = g_option_context_new ("- CLI tool of libssc for Qualcomm Sensor Core sensors");
	g_option_context_add_main_entries (opt_context, options, NULL);
	if (!g_option_context_parse (opt_context, &argc, &argv, &err)) {
		g_warning("Parsing CLI options failed: %s", err->message);
		return -1;
	}

	/* Print version and exit */
	if (print_version) {
		printf("libssc version %d.%d.%d\n", LIBSSC_MAJOR_VERSION, LIBSSC_MINOR_VERSION, LIBSSC_PATCH_VERSION);
		return 0;
	}

	g_info("libssc %d.%d.%d starting", LIBSSC_MAJOR_VERSION, LIBSSC_MINOR_VERSION, LIBSSC_PATCH_VERSION);

	/* Enable debug logs if requested */
	if (debug) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
		qmi_utils_set_traces_enabled (TRUE);
        	qmi_utils_set_show_personal_info (TRUE);
		g_debug("Debug messages enabled");
	}

	/* Read QMI device node */
	cli.device_str = g_strdup(device_str);
	g_debug("QMI device: %s", cli.device_str);

	/* Initialize QMI sensor client */
	file = g_file_new_for_commandline_arg (cli.device_str);
	if (!sensor_client_init (file))
		return EXIT_FAILURE;

	/* Start GLib main loop */
	cli.loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(cli.loop);

	return 0;
}
