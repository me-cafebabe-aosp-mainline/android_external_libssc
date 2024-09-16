/*
 * libssc: Library to expose Qualcomm Sensor Core sensors
 * Copyright (C) 2022-2024 Dylan Van Assche
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

#include <glib.h>
#include <locale.h>
#include "libssc-client.h"
#include "libssc-version.h"
#include "libssc-sensor.h"
#include "libssc-sensor-proximity.h"
#include "libssc-sensor-light.h"
#include "libssc-sensor-accelerometer.h"
#include "libssc-sensor-magnetometer.h"
#include "libssc-sensor-compass.h"

#define TIMEOUT 2

typedef struct {
	SSCSensor *sensor;
	GArray *measurements;
	GMainLoop *loop;
} SensorData;

typedef struct {
	gfloat x;
	gfloat y;
	gfloat z;
} XYZMeasurement;

static gboolean
proximity_close_cb (SensorData *data)
{
	SSCSensorProximity *self = SSC_SENSOR_PROXIMITY (data->sensor);
	GArray *measurements = data->measurements;
	GMainLoop *loop = data->loop;
	g_autoptr (GError) error = NULL;

	/* Close sensor */
	g_assert_true (ssc_sensor_proximity_close_sync (self, NULL, &error));

	/* 
	 * Check measurements: proximity sensor measurements which are
	 * the same as the previous value are skipped by libssc
	 */
	g_assert_cmpint (measurements->len, >=, 8);
	g_assert_true (g_array_index (measurements, gboolean, 0) == TRUE);
	g_assert_true (g_array_index (measurements, gboolean, 1) == FALSE);
	g_assert_true (g_array_index (measurements, gboolean, 2) == TRUE);
	g_assert_true (g_array_index (measurements, gboolean, 3) == FALSE);
	g_assert_true (g_array_index (measurements, gboolean, 4) == TRUE);
	g_assert_true (g_array_index (measurements, gboolean, 5) == FALSE);
	g_assert_true (g_array_index (measurements, gboolean, 6) == TRUE);
	g_assert_true (g_array_index (measurements, gboolean, 7) == FALSE);

	g_main_loop_quit (loop);

	return G_SOURCE_REMOVE;
}

static void
proximity_measurement (SSCSensorProximity *sensor, gboolean near, gpointer user_data)
{
	GArray *measurements = user_data;

	g_test_message("NEAR: %d", near);

	/* Collect measurement */
	g_array_append_val (measurements, near);
}

static void
test_libssc_sensor_proximity(void)
{
	g_autoptr (GError) error = NULL;
	GArray *measurements = g_array_new (FALSE, FALSE, sizeof (gboolean));
	GMainLoop *loop = g_main_loop_new (NULL, FALSE);
	SensorData data;

	/* Test information */
	g_test_summary ("Test `proximity sensor operations`");

	/* Create sensor */
	SSCSensorProximity *sensor = ssc_sensor_proximity_new_sync (NULL, &error);

	/* Connect measurement signal */
	g_signal_connect (sensor, "measurement", G_CALLBACK (proximity_measurement), measurements);

	/* Wait until all mocking measurements are received */
	data.sensor = SSC_SENSOR (sensor);
	data.measurements = measurements;
	data.loop = loop;
	g_timeout_add_seconds (TIMEOUT, (GSourceFunc)proximity_close_cb, &data);

	/* Open sensor */
	g_assert_true (ssc_sensor_proximity_open_sync (sensor, NULL, &error));

	/* Run main loop to process signals and timers */
	g_main_loop_run (loop);
}

static gboolean
light_close_cb (SensorData *data)
{
	SSCSensorLight *self = SSC_SENSOR_LIGHT (data->sensor);
	GArray *measurements = data->measurements;
	GMainLoop *loop = data->loop;
	g_autoptr (GError) error = NULL;

	/* Close sensor */
	g_assert_true (ssc_sensor_light_close_sync (self, NULL, &error));

	/* Check measurements */
	g_assert_cmpint (measurements->len, >=, 8);
	g_assert_cmpfloat (g_array_index (measurements, gfloat, 0), ==, 5.0);
	g_assert_cmpfloat (g_array_index (measurements, gfloat, 1), ==, 7.0);
	g_assert_cmpfloat (g_array_index (measurements, gfloat, 2), ==, 1.0);
	g_assert_cmpfloat (g_array_index (measurements, gfloat, 3), ==, 0.0);
	g_assert_cmpfloat (g_array_index (measurements, gfloat, 4), ==, 5.0);
	g_assert_cmpfloat (g_array_index (measurements, gfloat, 5), ==, 5.0);
	g_assert_cmpfloat (g_array_index (measurements, gfloat, 6), ==, 5.0);
	g_assert_cmpfloat (g_array_index (measurements, gfloat, 7), ==, 5.0);

	g_main_loop_quit (loop);

	return G_SOURCE_REMOVE;
}

static void
light_measurement (SSCSensorLight *sensor, gfloat intensity, gpointer user_data)
{
	GArray *measurements = user_data;

	g_test_message("Intensity: %f Lux", intensity);

	/* Collect measurement */
	g_array_append_val (measurements, intensity);
}

static void
test_libssc_sensor_light(void)
{
	g_autoptr (GError) error = NULL;
	GArray *measurements = g_array_new (FALSE, FALSE, sizeof (gfloat));
	GMainLoop *loop = g_main_loop_new (NULL, FALSE);
	SensorData data;

	/* Test information */
	g_test_summary ("Test `light sensor operations`");

	/* Create sensor */
	SSCSensorLight *sensor = ssc_sensor_light_new_sync (NULL, &error);

	/* Connect measurement signal */
	g_signal_connect (sensor, "measurement", G_CALLBACK (light_measurement), measurements);

	/* Wait until all mocking measurements are received */
	data.sensor = SSC_SENSOR (sensor);
	data.measurements = measurements;
	data.loop = loop;
	g_timeout_add_seconds (TIMEOUT, (GSourceFunc)light_close_cb, &data);

	/* Open sensor */
	g_assert_true (ssc_sensor_light_open_sync (sensor, NULL, &error));

	/* Run main loop to process signals and timers */
	g_main_loop_run (loop);
}

static gboolean
accelerometer_close_cb (SensorData *data)
{
	SSCSensorAccelerometer *self = SSC_SENSOR_ACCELEROMETER (data->sensor);
	GArray *measurements = data->measurements;
	GMainLoop *loop = data->loop;
	g_autoptr (GError) error = NULL;

	/* Close sensor */
	g_assert_true (ssc_sensor_accelerometer_close_sync (self, NULL, &error));

	/* Check measurements */
	g_assert_cmpint (measurements->len, >=, 20);

	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 0).x, ==, 0.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 0).y, ==, 0.0);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 0).z, -9.81, 0.001);

	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 1).x, ==, 2.5);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 1).y, ==, 1.5);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 1).z, ==, 0.0);

	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 2).x, ==, -2.5);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 2).y, ==, -1.5);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 2).z, ==, 0.0);

	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 3).x, ==, 0.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 3).y, ==, 0.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 3).z, ==, 0.0);

	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 4).x, ==, 0.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 4).y, ==, 0.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 4).z, ==, 0.0);

	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 5).x, ==, 1.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 5).y, ==, 1.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 5).z, ==, 1.0);

	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 6).x, ==, 1.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 6).z, ==, 1.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 6).y, ==, 1.0);

	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 7).x, ==, 1.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 7).y, ==, 1.0);
	g_assert_cmpfloat (g_array_index (measurements, XYZMeasurement, 7).z, ==, 1.0);

	g_main_loop_quit (loop);

	return G_SOURCE_REMOVE;
}

static void
accelerometer_measurement (SSCSensorAccelerometer *sensor, gfloat accel_x, gfloat accel_y, gfloat accel_z, gpointer user_data)
{
	GArray *measurements = user_data;
	XYZMeasurement measurement;

	g_test_message("Acceleration: X%f Y%f Z%f", accel_x, accel_y, accel_z);
	measurement.x = accel_x;
	measurement.y = accel_y;
	measurement.z = accel_z;

	/* Collect measurement */
	g_array_append_val (measurements, measurement);
}

static void
test_libssc_sensor_accelerometer(void)
{
	g_autoptr (GError) error = NULL;
	GArray *measurements = g_array_new (FALSE, FALSE, sizeof (XYZMeasurement));
	GMainLoop *loop = g_main_loop_new (NULL, FALSE);
	SensorData data;

	/* Test information */
	g_test_summary ("Test `accelerometer sensor operations`");

	/* Create sensor */
	SSCSensorAccelerometer *sensor = ssc_sensor_accelerometer_new_sync (NULL, &error);

	/* Connect measurement signal */
	g_signal_connect (sensor, "measurement", G_CALLBACK (accelerometer_measurement), measurements);

	/* Wait until all mocking measurements are received */
	data.sensor = SSC_SENSOR (sensor);
	data.measurements = measurements;
	data.loop = loop;
	g_timeout_add_seconds (TIMEOUT, (GSourceFunc)accelerometer_close_cb, &data);

	/* Open sensor */
	g_assert_true (ssc_sensor_accelerometer_open_sync (sensor, NULL, &error));

	/* Run main loop to process signals and timers */
	g_main_loop_run (loop);
}

static gboolean
compass_close_cb (SensorData *data)
{
	SSCSensorCompass *self = SSC_SENSOR_COMPASS (data->sensor);
	GArray *measurements = data->measurements;
	GMainLoop *loop = data->loop;
	g_autoptr (GError) error = NULL;

	/* Close sensor */
	g_assert_true (ssc_sensor_compass_close_sync (self, NULL, &error));

	/* Check measurements */
	g_assert_cmpint (measurements->len, >=, 20);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, gfloat, 0), 226.468811, 0.1);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, gfloat, 1), 113.62372, 0.1);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, gfloat, 2), 0.0, 0.1);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, gfloat, 3), 226.468811, 0.1);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, gfloat, 4), 226.468811, 0.1);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, gfloat, 5), 226.468811, 0.1);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, gfloat, 6), 226.468811, 0.1);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, gfloat, 7), 226.468811, 0.1);

	g_main_loop_quit (loop);

	return G_SOURCE_REMOVE;
}

static void
compass_measurement (SSCSensorCompass *sensor, gfloat heading, gpointer user_data)
{
	GArray *measurements = user_data;

	g_test_message("Compass: %f degrees", heading);

	/* Collect measurement */
	g_array_append_val (measurements, heading);
}

static void
test_libssc_sensor_compass(void)
{
	g_autoptr (GError) error = NULL;
	GArray *measurements = g_array_new (FALSE, FALSE, sizeof (gfloat));
	GMainLoop *loop = g_main_loop_new (NULL, FALSE);
	SensorData data;

	/* Test information */
	g_test_summary ("Test `compass sensor operations`");

	/* Create sensor */
	SSCSensorCompass *sensor = ssc_sensor_compass_new_sync (NULL, &error);

	/* Connect measurement signal */
	g_signal_connect (sensor, "measurement", G_CALLBACK (compass_measurement), measurements);

	/* Wait until all mocking measurements are received */
	data.sensor = SSC_SENSOR (sensor);
	data.measurements = measurements;
	data.loop = loop;
	g_timeout_add_seconds (TIMEOUT, (GSourceFunc)compass_close_cb, &data);

	/* Open sensor */
	g_assert_true (ssc_sensor_compass_open_sync (sensor, NULL, &error));

	/* Run main loop to process signals and timers */
	g_main_loop_run (loop);
}

static gboolean
magnetometer_close_cb (SensorData *data)
{
	SSCSensorMagnetometer *self = SSC_SENSOR_MAGNETOMETER (data->sensor);
	GArray *measurements = data->measurements;
	GMainLoop *loop = data->loop;
	g_autoptr (GError) error = NULL;

	/* Close sensor */
	g_assert_true (ssc_sensor_magnetometer_close_sync (self, NULL, &error));

	/* Check measurements */
	g_assert_cmpint (measurements->len, >=, 20);

	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 0).x, 0.1, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 0).y, 0.2, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 0).z, 0.3, 0.01);

	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 1).x, 0.3, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 1).y, 0.2, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 1).z, 0.1, 0.01);

	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 2).x, 0.3, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 2).y, 0.2, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 2).z, 0.1, 0.01);

	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 3).x, 0.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 3).y, 0.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 3).z, 0.0, 0.01);

	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 4).x, 0.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 4).y, 0.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 4).z, 0.0, 0.01);

	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 5).x, 1.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 5).y, 1.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 5).z, 1.0, 0.01);

	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 6).x, 1.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 6).y, 1.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 6).z, 1.0, 0.01);

	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 7).x, 1.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 7).y, 1.0, 0.01);
	g_assert_cmpfloat_with_epsilon (g_array_index (measurements, XYZMeasurement, 7).z, 1.0, 0.01);

	g_main_loop_quit (loop);

	return G_SOURCE_REMOVE;
}

static void
magnetometer_measurement (SSCSensorMagnetometer *sensor, gfloat magn_x, gfloat magn_y, gfloat magn_z, gpointer user_data)
{
	GArray *measurements = user_data;
	XYZMeasurement measurement;

	g_test_message("Magnetic field: X%f Y%f Z%f", magn_x, magn_y, magn_z);
	measurement.x = magn_x;
	measurement.y = magn_y;
	measurement.z = magn_z;

	/* Collect measurement */
	g_array_append_val (measurements, measurement);
}

static void
test_libssc_sensor_magnetometer(void)
{
	g_autoptr (GError) error = NULL;
	GArray *measurements = g_array_new (FALSE, FALSE, sizeof (XYZMeasurement));
	GMainLoop *loop = g_main_loop_new (NULL, FALSE);
	SensorData data;

	/* Test information */
	g_test_summary ("Test `magnetometer sensor operations`");

	/* Create sensor */
	SSCSensorMagnetometer *sensor = ssc_sensor_magnetometer_new_sync (NULL, &error);

	/* Connect measurement signal */
	g_signal_connect (sensor, "measurement", G_CALLBACK (magnetometer_measurement), measurements);

	/* Wait until all mocking measurements are received */
	data.sensor = SSC_SENSOR (sensor);
	data.measurements = measurements;
	data.loop = loop;
	g_timeout_add_seconds (TIMEOUT, (GSourceFunc)magnetometer_close_cb, &data);

	/* Open sensor */
	g_assert_true (ssc_sensor_magnetometer_open_sync (sensor, NULL, &error));

	/* Run main loop to process signals and timers */
	g_main_loop_run (loop);
}
int main (int argc, char *argv[])
{
	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);

	/* Tests */
	g_test_add_func("/libssc/sensor/proximity", test_libssc_sensor_proximity);
	g_test_add_func("/libssc/sensor/light", test_libssc_sensor_light);
	g_test_add_func("/libssc/sensor/accelerometer", test_libssc_sensor_accelerometer);
	g_test_add_func("/libssc/sensor/compass", test_libssc_sensor_compass);
	g_test_add_func("/libssc/sensor/magnetometer", test_libssc_sensor_magnetometer);

	return g_test_run();
}
