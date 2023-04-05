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

#include "libssc-common.h"

void
ssc_common_dump_protobuf (GArray *protobuf)
{
	if (protobuf == NULL) {
		g_warning ("Invalid ProtoBuf data!");
		return;
	}

	g_debug ("ProtoBuf data:");
	for (gsize i = 0; i < protobuf->len; i++) {
		guint8 value = g_array_index (protobuf, guint8, i);
		g_printf ("\\x%02x", value);
	}
	g_printf ("\n");
}
