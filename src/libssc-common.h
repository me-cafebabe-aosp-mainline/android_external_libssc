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

#ifndef _LIBSSC_COMMON_H_
#define _LIBSSC_COMMON_H_

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gio/gio.h>
#include <libqmi-glib.h>

#define SSC_MSG_REQUEST_ENABLE_REPORT_ON_CHANGE		514
#define SSC_MSG_REQUEST_ENABLE_REPORT_CONTINUOUS 	513
#define SSC_MSG_REQUEST_DISABLE_REPORT 			10
#define SSC_MSG_REQUEST_GET_ATTRIBUTES			1
#define SSC_MSG_RESPONSE_GET_ATTRIBUTES			128
#define SSC_ACCURACY_UNRELIABLE				0
#define SSC_ACCURACY_LOW				1
#define SSC_ACCURACY_MEDIUM				2
#define SSC_ACCURACY_HIGH				3
#define SSC_ATTRIBUTE_NAME				0
#define SSC_ATTRIBUTE_VENDOR				1
#define SSC_ATTRIBUTE_AVAILABLE				3
#define SSC_ATTRIBUTE_SAMPLE_RATE			6
#define SSC_ATTRIBUTE_STREAM_TYPE			16
#define SSC_STREAM_TYPE_CONTINUOUS			0
#define SSC_STREAM_TYPE_ON_CHANGE			1

void
ssc_common_dump_protobuf (GArray *protobuf);

#endif /* _LIBSSC_COMMON_H_ */
