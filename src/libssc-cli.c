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

#include "libssc-cli.h"

#define ENABLE_SECONDS 3
#define GENERAL_FAIL_EXIT_CODE -1
#define INIT_FAIL_EXIT_CODE -2
#define OPEN_FAIL_EXIT_CODE -3
#define CLOSE_FAIL_EXIT_CODE -4

static gboolean
magnetometer_close_cb (SSCSensorMagnetometer *self)
{
	g_autoptr (GError) err = NULL;

	if (!ssc_sensor_magnetometer_close_sync (self, NULL, &err)) {
		g_warning ("Unable to close magnetometer sensor: %s", err ? err->message : "NULL");
	}

	g_debug ("Magnetometer sensor disabled");
	exit(0);

	return G_SOURCE_REMOVE;
}

static void magnetometer_measurement (SSCSensorMagnetometer *sensor, gdouble magnetic_field_x, gdouble magnetic_field_y, gdouble magnetic_field_z, gpointer user_data)
{
	g_printf ("Magnetometer sensor measurement: X=%f Y=%f Z=%f μT\n", magnetic_field_x, magnetic_field_y, magnetic_field_z);
}

/*****************************************************************************/

static gboolean
accelerometer_close_cb (SSCSensorAccelerometer *self)
{
	g_autoptr (GError) err = NULL;

	if (!ssc_sensor_accelerometer_close_sync (self, NULL, &err)) {
		g_warning ("Unable to close accelerometer sensor: %s", err ? err->message : "NULL");
	}

	g_debug ("Accelerometer sensor disabled");
	exit(0);

	return G_SOURCE_REMOVE;
}

static void accelerometer_measurement (SSCSensorAccelerometer *sensor, gdouble accel_x, gdouble accel_y, gdouble accel_z, gpointer user_data)
{
	g_printf ("Accelerometer sensor measurement: X=%f Y=%f Z=%f m/s²\n", accel_x, accel_y, accel_z);
}

/*****************************************************************************/

static gboolean
light_close_cb (SSCSensorLight *self)
{
	g_autoptr (GError) err = NULL;

	if (!ssc_sensor_light_close_sync (self, NULL, &err)) {
		g_warning ("Unable to close light sensor: %s", err ? err->message : "NULL");
	}

	g_debug ("Light sensor disabled");
	exit(0);

	return G_SOURCE_REMOVE;
}

static void light_measurement (SSCSensorLight *sensor, gdouble intensity, gpointer user_data)
{
	g_printf ("Light sensor measurement: %f Lux\n", intensity);
}

/*****************************************************************************/

static gboolean
proximity_close_cb (SSCSensorProximity *self)
{
	g_autoptr (GError) err = NULL;

	if (!ssc_sensor_proximity_close_sync (self, NULL, &err)) {
		g_warning ("Unable to close proximity sensor: %s", err ? err->message : "NULL");
	}

	g_debug ("Proximity sensor disabled");
	exit(0);

	return G_SOURCE_REMOVE;
}

static void proximity_measurement (SSCSensorProximity *sensor, gboolean near, gpointer user_data)
{
	g_printf ("Proximity sensor measurement: %s\n", near ? "NEAR" : "FAR");
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	g_autoptr(GOptionContext) opt_context = NULL;
	GError *err = NULL;
	GFile *file = NULL;
	g_autofree gchar *device_str = "qrtr://0";
	SSCCli cli;
	gboolean print_version = FALSE;
	gboolean debug = FALSE;
	g_autofree gchar *sensor_str = "";
	const GOptionEntry options[] = {
		{ "version", 0, 0, G_OPTION_ARG_NONE, &print_version, "Print version information and exit.", NULL },
		{ "device", 0, 0, G_OPTION_ARG_STRING, &device_str, "QMI device to use, default 'qrtr://0'.", NULL },
		{ "debug", 'v', 0, G_OPTION_ARG_NONE, &debug, "Enable debug logs.", NULL },
		{ "sensor", 0, 0, G_OPTION_ARG_STRING, &sensor_str, "Enable a sensor. Supported sensors: 'proximity', 'light', 'accelerometer', 'magnetometer'", NULL },
		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};

	opt_context = g_option_context_new ("- CLI tool of libssc for Qualcomm Sensor Core sensors");
	g_option_context_add_main_entries (opt_context, options, NULL);
	if (!g_option_context_parse (opt_context, &argc, &argv, &err)) {
		g_warning ("Parsing CLI options failed: %s", err->message);
		return GENERAL_FAIL_EXIT_CODE;
	}

	/* Print version and exit */
	if (print_version) {
		printf ("libssc version %d.%d.%d\n", LIBSSC_MAJOR_VERSION, LIBSSC_MINOR_VERSION, LIBSSC_PATCH_VERSION);
		return 0;
	}

	g_info("libssc %d.%d.%d starting", LIBSSC_MAJOR_VERSION, LIBSSC_MINOR_VERSION, LIBSSC_PATCH_VERSION);

	/* Enable debug logs if requested */
	if (debug) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
		qmi_utils_set_traces_enabled (TRUE);
        	qmi_utils_set_show_personal_info (TRUE);
		g_debug ("Debug messages enabled");
	}

	/* Read QMI device node */
	cli.device_str = g_strdup(device_str);
	file = g_file_new_for_commandline_arg (cli.device_str);
	g_debug ("QMI device: %s", cli.device_str);

	if (g_strcmp0 (sensor_str, "proximity") == 0) {
		SSCSensorProximity *proximity = ssc_sensor_proximity_new_sync (file, NULL, &err);
		if (!proximity) {
			g_warning ("Unable to initialize proximity sensor: %s", err ? err->message : "NULL");
			return INIT_FAIL_EXIT_CODE;
		}
		g_signal_connect (proximity,
			  	  "measurement",
			  	  G_CALLBACK (proximity_measurement),
			  	  NULL);
		if (!ssc_sensor_proximity_open_sync (proximity, NULL, &err)) {
			g_warning ("Unable to open proximity sensor: %s", err ? err->message : "UNKNOWN");
			return OPEN_FAIL_EXIT_CODE;
		}
		g_timeout_add_seconds (ENABLE_SECONDS, (GSourceFunc)proximity_close_cb, proximity);
	} else if (g_strcmp0 (sensor_str, "light") == 0) {
		SSCSensorLight *light = ssc_sensor_light_new_sync (file, NULL, &err);
		if (!light) {
			g_warning ("Unable to initialize light sensor: %s", err ? err->message : "NULL");
			return INIT_FAIL_EXIT_CODE;
		}
		g_signal_connect (light,
			  	  "measurement",
			  	  G_CALLBACK (light_measurement),
			  	  NULL);
		if (!ssc_sensor_light_open_sync (light, NULL, &err)) {
			g_warning ("Unable to open light sensor: %s", err ? err->message : "UNKNOWN");
			return OPEN_FAIL_EXIT_CODE;
		}
		g_timeout_add_seconds (ENABLE_SECONDS, (GSourceFunc)light_close_cb, light);
	} else if (g_strcmp0 (sensor_str, "accelerometer") == 0) {
		SSCSensorAccelerometer *accelerometer = ssc_sensor_accelerometer_new_sync (file, NULL, &err);
		if (!accelerometer) {
			g_warning ("Unable to initialize accelerometer sensor: %s", err ? err->message : "NULL");
			return INIT_FAIL_EXIT_CODE;
		}
		g_signal_connect (accelerometer,
			  	  "measurement",
			  	  G_CALLBACK (accelerometer_measurement),
			  	  NULL);
		if (!ssc_sensor_accelerometer_open_sync (accelerometer, NULL, &err)) {
			g_warning ("Unable to open accelerometer sensor: %s", err ? err->message : "UNKNOWN");
			return OPEN_FAIL_EXIT_CODE;
		}
		g_timeout_add_seconds (ENABLE_SECONDS, (GSourceFunc)accelerometer_close_cb, accelerometer);
	} else if (g_strcmp0 (sensor_str, "magnetometer") == 0) {
		SSCSensorMagnetometer *magnetometer = ssc_sensor_magnetometer_new_sync (file, NULL, &err);
		if (!magnetometer) {
			g_warning ("Unable to initialize magnetometer sensor: %s", err ? err->message : "NULL");
			return INIT_FAIL_EXIT_CODE;
		}
		g_signal_connect (magnetometer,
			  	  "measurement",
			  	  G_CALLBACK (magnetometer_measurement),
			  	  NULL);
		if (!ssc_sensor_magnetometer_open_sync (magnetometer, NULL, &err)) {
			g_warning ("Unable to open magnetometer sensor: %s", err ? err->message : "UNKNOWN");
			return OPEN_FAIL_EXIT_CODE;
		}
		g_timeout_add_seconds (ENABLE_SECONDS, (GSourceFunc)magnetometer_close_cb, magnetometer);
	} else {
		g_printf ("Specify a supported sensor: 'proximity', 'light', 'accelerometer', 'magnetometer'\n");
		return GENERAL_FAIL_EXIT_CODE;
	}

	/* Start GLib main loop */
	cli.loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (cli.loop);

	return 0;
}
