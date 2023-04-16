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

static void
magnetometer_close_ready (SSCSensorMagnetometer *sensor, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;

	if (!ssc_sensor_magnetometer_close_finish (sensor, result, &error)) {
		g_warning ("Failed to close magnetometer sensor");
		return;
	}

	g_debug ("Magnetometer sensor disabled");
}

static gboolean
magnetometer_close_cb (SSCSensorMagnetometer *self)
{
	ssc_sensor_magnetometer_close (self, NULL, (GAsyncReadyCallback)magnetometer_close_ready, NULL);

	return G_SOURCE_REMOVE;
}

static void magnetometer_measurement (SSCSensorMagnetometer *sensor, gfloat magnetic_field_x, gfloat magnetic_field_y, gfloat magnetic_field_z, gpointer user_data)
{
	g_printf ("Magnetometer measurement: X=%f Y=%f Z=%f μT\n", magnetic_field_x, magnetic_field_y, magnetic_field_z);
}

static void
magnetometer_open_ready (SSCSensorMagnetometer *sensor, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;

	if (!ssc_sensor_magnetometer_open_finish (sensor, result, &error)) {
		g_warning ("Failed to open magnetometer sensor");
		return;
	}

	g_debug ("Magnetometer sensor enabled");
	g_timeout_add_seconds (1, (GSourceFunc)magnetometer_close_cb, sensor);
}

static void
magnetometer_ready (GFile *self, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;
	SSCSensorMagnetometer *sensor = NULL;

	sensor = ssc_sensor_magnetometer_new_finish (result, &error);

	if (sensor)
		g_debug ("Magnetometer Sensor allocated");
	else {
		g_debug ("Magnetometer sensor is NULL");
		return;
	}

	g_signal_connect (SSC_SENSOR_MAGNETOMETER (sensor),
			  "measurement",
			  G_CALLBACK (magnetometer_measurement),
			  NULL);

	g_debug ("Magnetometer sensor enabling");
	ssc_sensor_magnetometer_open (sensor, NULL, (GAsyncReadyCallback)magnetometer_open_ready, NULL);
}

/*****************************************************************************/
static void
accelerometer_close_ready (SSCSensorAccelerometer *sensor, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;

	if (!ssc_sensor_accelerometer_close_finish (sensor, result, &error)) {
		g_warning ("Failed to close accelerometer sensor");
		return;
	}

	g_debug ("Accelerometer sensor disabled");
}

static gboolean
accelerometer_close_cb (SSCSensorAccelerometer *self)
{
	ssc_sensor_accelerometer_close (self, NULL, (GAsyncReadyCallback)accelerometer_close_ready, NULL);

	return G_SOURCE_REMOVE;
}

static void accelerometer_measurement (SSCSensorAccelerometer *sensor, gfloat accel_x, gfloat accel_y, gfloat accel_z, gpointer user_data)
{
	g_printf ("Accelerometer measurement: X=%f Y=%f Z=%f m/s²\n", accel_x, accel_y, accel_z);
}

static void
accelerometer_open_ready (SSCSensorAccelerometer *sensor, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;

	if (!ssc_sensor_accelerometer_open_finish (sensor, result, &error)) {
		g_warning ("Failed to open accelerometer sensor");
		return;
	}

	g_debug ("Accelerometer sensor enabled");
	g_timeout_add_seconds (1, (GSourceFunc)accelerometer_close_cb, sensor);
}

static void
accelerometer_ready (GFile *self, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;
	SSCSensorAccelerometer *sensor = NULL;

	sensor = ssc_sensor_accelerometer_new_finish (result, &error);

	if (sensor)
		g_debug ("Accelerometer Sensor allocated");
	else {
		g_debug ("Accelerometer sensor is NULL");
		return;
	}

	g_signal_connect (SSC_SENSOR_ACCELEROMETER (sensor),
			  "measurement",
			  G_CALLBACK (accelerometer_measurement),
			  NULL);

	g_debug ("Accelerometer sensor enabling");
	ssc_sensor_accelerometer_open (sensor, NULL, (GAsyncReadyCallback)accelerometer_open_ready, NULL);
}

/*****************************************************************************/

static void
light_close_ready (SSCSensorLight *sensor, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;

	if (!ssc_sensor_light_close_finish (sensor, result, &error)) {
		g_warning ("Failed to close light sensor");
		return;
	}

	g_debug ("Light sensor disabled");
}

static gboolean
light_close_cb (SSCSensorLight *self)
{
	ssc_sensor_light_close (self, NULL, (GAsyncReadyCallback)light_close_ready, NULL);

	return G_SOURCE_REMOVE;
}

static void light_measurement (SSCSensorLight *sensor, gfloat intensity, gpointer user_data)
{
	g_printf ("Light measurement: %f Lux\n", intensity);
}

static void
light_open_ready (SSCSensorLight *sensor, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;

	if (!ssc_sensor_light_open_finish (sensor, result, &error)) {
		g_warning ("Failed to open light sensor");
		return;
	}

	g_debug ("Light sensor enabled");
	g_timeout_add_seconds (1, (GSourceFunc)light_close_cb, sensor);
}

static void
light_ready (GFile *self, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;
	SSCSensorLight *sensor = NULL;

	sensor = ssc_sensor_light_new_finish (result, &error);

	if (sensor)
		g_debug ("Light Sensor allocated");
	else {
		g_debug ("Light sensor is NULL");
		return;
	}

	g_signal_connect (SSC_SENSOR_LIGHT (sensor),
			  "measurement",
			  G_CALLBACK (light_measurement),
			  NULL);

	g_debug ("Light sensor enabling");
	ssc_sensor_light_open (sensor, NULL, (GAsyncReadyCallback)light_open_ready, NULL);
}

/*****************************************************************************/

static void
proximity_close_ready (SSCSensorProximity *sensor, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;

	if (!ssc_sensor_proximity_close_finish (sensor, result, &error)) {
		g_warning ("Failed to close proximity sensor");
		return;
	}

	g_debug ("Proximity sensor disabled");
}

static gboolean
proximity_close_cb (SSCSensorProximity *self)
{
	ssc_sensor_proximity_close (self, NULL, (GAsyncReadyCallback)proximity_close_ready, NULL);

	return G_SOURCE_REMOVE;
}

static void proximity_measurement (SSCSensorProximity *sensor, gboolean near, gpointer user_data)
{
	g_printf ("Proximity measurement: %s\n", near ? "NEAR" : "FAR");
}


static void
proximity_open_ready (SSCSensorProximity *sensor, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;

	if (!ssc_sensor_proximity_open_finish (sensor, result, &error)) {
		g_warning ("Failed to open proximity sensor");
		return;
	}

	g_debug ("Proximity sensor enabled");
	g_timeout_add_seconds (1, (GSourceFunc)proximity_close_cb, sensor);
}

static void
proximity_ready (GFile *self, GAsyncResult *result, gpointer user_data)
{
	g_autoptr (GError) error = NULL;
	SSCSensorProximity *sensor = NULL;

	sensor = ssc_sensor_proximity_new_finish (result, &error);

	if (sensor)
		g_debug ("Proximity Sensor allocated");
	else {
		g_debug ("Proximity sensor is NULL");
		return;
	}

	g_signal_connect (SSC_SENSOR_PROXIMITY (sensor),
			  "measurement",
			  G_CALLBACK (proximity_measurement),
			  NULL);

	g_debug ("Proximity sensor enabling");
	ssc_sensor_proximity_open (sensor, NULL, (GAsyncReadyCallback)proximity_open_ready, NULL);
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	g_autoptr(GOptionContext) opt_context = NULL;
	g_autoptr(GError) err = NULL;
	GFile *file = NULL;
	g_autofree gchar *device_str = "qrtr://0";
	SSCCli cli;
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
		g_warning ("Parsing CLI options failed: %s", err->message);
		return -1;
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
	g_debug ("QMI device: %s", cli.device_str);

	/* Start GLib main loop */
	cli.loop = g_main_loop_new(NULL, FALSE);

	/* Initialize QMI sensor client */
	file = g_file_new_for_commandline_arg (cli.device_str);
	//ssc_sensor_proximity_new (file, NULL, (GAsyncReadyCallback)proximity_ready, NULL);
	//ssc_sensor_light_new (file, NULL, (GAsyncReadyCallback)light_ready, NULL);
	//ssc_sensor_accelerometer_new (file, NULL, (GAsyncReadyCallback)accelerometer_ready, NULL);
	ssc_sensor_magnetometer_new (file, NULL, (GAsyncReadyCallback)magnetometer_ready, NULL);

	g_main_loop_run(cli.loop);

	return 0;
}
