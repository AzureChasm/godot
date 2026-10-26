/**************************************************************************/
/*  terrain_heightmap.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "terrain_heightmap.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"

bool TerrainHeightmap::write_csv(const TerrainData &p_data, const String &p_path, int p_stride, int64_t p_request_id, Dictionary &r_statistics, String &r_error, const SafeFlag *p_cancel) {
	if (p_data.sections.is_empty() || p_stride < 1 || p_stride > 32) {
		r_error = "No completed terrain cache or invalid heightmap stride.";
		return false;
	}
	const Error directory_error = DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	if (directory_error != OK && directory_error != ERR_ALREADY_EXISTS) {
		r_error = "Cannot create heightmap output directory.";
		return false;
	}
	static SafeNumeric<uint64_t> serial;
	const String temporary = p_path + ".tmp." + itos(OS::get_singleton()->get_process_id()) + "." + itos(serial.increment());
	Ref<FileAccess> file = FileAccess::open(temporary, FileAccess::WRITE);
	if (file.is_null()) {
		r_error = "Cannot open heightmap output: " + p_path;
		return false;
	}
	const real_t spacing = p_data.spacing;
	const Vector3 end = p_data.bounds.get_end();
	const Vector3i first(Math::ceil(p_data.bounds.position.x / spacing), Math::ceil(p_data.bounds.position.y / spacing), Math::ceil(p_data.bounds.position.z / spacing));
	const Vector3i last(Math::floor(end.x / spacing), Math::floor(end.y / spacing), Math::floor(end.z / spacing));
	Dictionary metadata;
	metadata["request_id"] = p_request_id;
	metadata["source"] = "cached_signed_field_before_mesh_lod";
	metadata["spacing_m"] = spacing * p_stride;
	metadata["vertical_spacing_m"] = spacing;
	metadata["top_surface_only"] = true;
	metadata["y_min_m"] = first.y * spacing;
	metadata["y_max_m"] = last.y * spacing;
	file->store_line("# " + JSON::stringify(metadata));
	file->store_line("x,z,height,status");
	HashMap<Vector3i, int> lookup;
	Vector3i minimum = p_data.sections[0].address, maximum = minimum;
	for (int i = 0; i < p_data.sections.size(); ++i) {
		const Vector3i address = p_data.sections[i].address;
		lookup.insert(address, i);
		for (int axis = 0; axis < 3; ++axis) {
			minimum[axis] = MIN(minimum[axis], address[axis]);
			maximum[axis] = MAX(maximum[axis], address[axis]);
		}
	}
	int valid = 0, clipped = 0, empty = 0;
	const int cells = p_data.section_cells;
	for (int z = first.z; z <= last.z; z += p_stride) {
		if (p_cancel && p_cancel->is_set()) {
			file.unref();
			DirAccess::remove_absolute(temporary);
			return false;
		}
		for (int x = first.x; x <= last.x; x += p_stride) {
			String status = "no_surface";
			real_t height = 0;
			float above = 0;
			for (int y = last.y; y >= first.y; --y) {
				const Vector3i address(CLAMP(int(Math::floor(real_t(x) / cells)), minimum.x, maximum.x), CLAMP(int(Math::floor(real_t(y) / cells)), minimum.y, maximum.y), CLAMP(int(Math::floor(real_t(z) / cells)), minimum.z, maximum.z));
				const int *index = lookup.getptr(address);
				if (!index) {
					r_error = "Heightmap encountered missing cached section.";
					file.unref();
					DirAccess::remove_absolute(temporary);
					return false;
				}
				const TerrainSection &section = p_data.sections[*index];
				const float value = section.field_at(x - address.x * cells, y - address.y * cells, z - address.z * cells);
				if (y == last.y && value <= 0) {
					status = "clipped_top";
					++clipped;
					break;
				}
				if (value <= 0) {
					// First air-to-solid crossing from above, interpolated on the source lattice.
					const real_t fraction = above > value ? -value / (above - value) : 0;
					height = (y + fraction) * spacing;
					status = "valid";
					++valid;
					break;
				}
				above = value;
			}
			if (status == "no_surface") {
				++empty;
			}
			file->store_line(String::num(x * spacing, 8) + "," + String::num(z * spacing, 8) + "," + (status == "valid" ? String::num(height, 8) : String()) + "," + status);
		}
	}
	file->flush();
	const Error write_error = file->get_error();
	file.unref();
	if (write_error != OK || (p_cancel && p_cancel->is_set())) {
		DirAccess::remove_absolute(temporary);
		if (write_error != OK) {
			r_error = "Heightmap write failed.";
		}
		return false;
	}
	if (DirAccess::rename_absolute(temporary, p_path) != OK) {
		DirAccess::remove_absolute(temporary);
		r_error = "Cannot publish heightmap CSV: " + p_path;
		return false;
	}
	r_statistics["request_id"] = p_request_id;
	r_statistics["path"] = p_path;
	r_statistics["valid_columns"] = valid;
	r_statistics["clipped_top_columns"] = clipped;
	r_statistics["no_surface_columns"] = empty;
	r_statistics["spacing_m"] = spacing * p_stride;
	return true;
}
