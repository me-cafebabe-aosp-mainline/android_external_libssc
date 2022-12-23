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

#include "ssc-sensor-proxy.h"
#include "proxy-version.h"

int main(int argc, char *argv[])
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GError) err = NULL;
	struct SSCSensorProxy proxy;
	gboolean print_version = FALSE;
	gboolean debug = FALSE;
	const GOptionEntry options[] = {
		{ "version", 'v', 0, G_OPTION_ARG_NONE, &print_version, "Print version information and exit.", NULL },
		{ "debug", 'v', 0, G_OPTION_ARG_NONE, &debug, "Enable debug logs.", NULL },
		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};

	opt_context = g_option_context_new ("- iio-sensor-proxy for Qualcomm Sensor Core");
	g_option_context_add_main_entries (opt_context, options, NULL);
	if (!g_option_context_parse (opt_context, &argc, &argv, &err)) {
		g_warning("Parsing CLI options failed: %s", err->message);
		return -1;
	}

	/* Print version and exit */
	if (print_version) {
		printf("ssc-sensor-proxy version %d.%d.%d\n", PROXY_MAJOR_VERSION, PROXY_MINOR_VERSION, PROXY_PATCH_VERSION);
		return 0;
	}

	g_info("ssc-sensor-proxy %d.%d.%d starting", PROXY_MAJOR_VERSION, PROXY_MINOR_VERSION, PROXY_PATCH_VERSION);

	/* Enable debug logs if requested */
	if (debug)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
		g_debug("Debug messages enabled");

	/* Start GLib main loop */
	proxy.loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(proxy.loop);

	return 0;
}
