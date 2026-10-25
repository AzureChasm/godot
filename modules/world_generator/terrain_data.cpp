/**************************************************************************/
/*  terrain_data.cpp                                                      */
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

#include "terrain_data.h"

#include "core/math/math_funcs.h"

namespace {
struct OperationLess {
	bool operator()(const TerrainOperation &p_a, const TerrainOperation &p_b) const {
		return p_a.priority == p_b.priority ? p_a.sequence < p_b.sequence : p_a.priority < p_b.priority;
	}
};
bool inside_inclusive(const AABB &p_box, const Vector3 &p_point) {
	const Vector3 end = p_box.get_end();
	return p_point.x >= p_box.position.x && p_point.y >= p_box.position.y && p_point.z >= p_box.position.z && p_point.x <= end.x && p_point.y <= end.y && p_point.z <= end.z;
}
} // namespace

TerrainSample TerrainData::sample(const Vector3 &p_position) const {
	TerrainSample result;
	result.field = p_position.y;
	for (const TerrainOperation &operation : operations) {
		bool in_scope = true;
		for (const AABB &scope : operation.scopes) {
			if (!inside_inclusive(scope, p_position)) {
				in_scope = false;
				break;
			}
		}
		if (in_scope) {
			operation.modifier->apply(p_position, result.field, result.material);
		}
	}
	return result;
}

Vector3 TerrainData::normal(const Vector3 &p_position) const {
	const real_t epsilon = spacing * 0.2;
	const Vector3 dx(epsilon, 0, 0);
	const Vector3 dy(0, epsilon, 0);
	const Vector3 dz(0, 0, epsilon);
	Vector3 gradient(sample(p_position + dx).field - sample(p_position - dx).field,
			sample(p_position + dy).field - sample(p_position - dy).field,
			sample(p_position + dz).field - sample(p_position - dz).field);
	return gradient.length_squared() > 1e-12 ? gradient.normalized() : Vector3(0, 1, 0);
}

bool TerrainData::prepare(String &r_error) {
	if (!bounds.position.is_finite() || !bounds.size.is_finite() || bounds.size.x <= 0 || bounds.size.y <= 0 || bounds.size.z <= 0 ||
			!Math::is_finite(spacing) || spacing < 0.025 || spacing > 2 || section_cells < 4 || section_cells > 64) {
		r_error = "Invalid preview bounds, sample spacing (0.025..2), or section cells (4..64).";
		return false;
	}
	// Deliberate local-preview safety limit, not the production world coordinate format.
	const Vector3 end = bounds.get_end();
	if (bounds.position.abs().length() > 100000 || end.abs().length() > 100000) {
		r_error = "Prototype preview must be within 100000 metres of the origin.";
		return false;
	}
	// A world chunk always spans 16 metres. Snap the sample lattice to a whole
	// number of cells, then select a nearby section divisor (sections are internal).
	const int world_cells = int(Math::ceil(WORLD_CHUNK_SIZE / spacing));
	spacing = WORLD_CHUNK_SIZE / world_cells;
	int best = 4;
	for (int cells = 4; cells <= 64; ++cells) {
		if (world_cells % cells == 0 && Math::abs(cells - section_cells) <= Math::abs(best - section_cells)) {
			best = cells;
		}
	}
	if (world_cells % best != 0) {
		// A prime cell count may have no supported section divisor. Choose
		// a finer lattice rather than crossing world chunk ownership.
		const int subdivisions = MAX(1, int(Math::ceil(real_t(world_cells) / section_cells)));
		section_cells = int(Math::ceil(real_t(world_cells) / subdivisions));
		spacing = WORLD_CHUNK_SIZE / (section_cells * subdivisions);
	} else {
		section_cells = best;
	}
	const Vector3i world_first(Math::floor(bounds.position.x / WORLD_CHUNK_SIZE), Math::floor(bounds.position.y / WORLD_CHUNK_SIZE), Math::floor(bounds.position.z / WORLD_CHUNK_SIZE));
	const Vector3i world_last(Math::ceil(end.x / WORLD_CHUNK_SIZE) - 1, Math::ceil(end.y / WORLD_CHUNK_SIZE) - 1, Math::ceil(end.z / WORLD_CHUNK_SIZE) - 1);
	const Vector3i world_count = world_last - world_first + Vector3i(1, 1, 1);
	world_chunk_count = int64_t(world_count.x) * world_count.y * world_count.z;
	const real_t section_size = spacing * section_cells;
	const Vector3i first(Math::floor(bounds.position.x / section_size), Math::floor(bounds.position.y / section_size), Math::floor(bounds.position.z / section_size));
	const Vector3i last(Math::ceil(end.x / section_size) - 1, Math::ceil(end.y / section_size) - 1, Math::ceil(end.z / section_size) - 1);
	const Vector3i count = last - first + Vector3i(1, 1, 1);
	const int64_t section_count = int64_t(count.x) * count.y * count.z;
	const int side = section_cells + 3;
	const int sample_count = side * side * side;
	if (section_count <= 0 || section_count > 4096) {
		r_error = vformat("Preview requires %d sections; maximum is 4096. Reduce view distance or increase spacing.", section_count);
		return false;
	}
	if (section_count * sample_count > sample_budget) {
		r_error = vformat("Preview requires %d samples; limit is %d. Reduce view distance or increase spacing.", section_count * sample_count, sample_budget);
		return false;
	}
	operations.sort_custom<OperationLess>();
	column_profile = true;
	for (const TerrainOperation &operation : operations) {
		column_profile &= operation.modifier->is_column_profile();
		for (const AABB &scope : operation.scopes) {
			// The shortcut is valid only when scope participation is constant in Y.
			column_profile &= (scope.position.y <= bounds.position.y && scope.get_end().y >= end.y) || scope.get_end().y < bounds.position.y || scope.position.y > end.y;
		}
	}
	column_samples = 0;
	sections.clear();
	sections.reserve(section_count);
	for (int cz = first.z; cz <= last.z; ++cz) {
		for (int cy = first.y; cy <= last.y; ++cy) {
			for (int cx = first.x; cx <= last.x; ++cx) {
				TerrainSection section;
				section.address = Vector3i(cx, cy, cz);
				section.origin = Vector3(cx, cy, cz) * section_size;
				section.cells = section_cells;

				sections.push_back(section);
			}
		}
	}
	column_samples = column_profile ? sections.size() * side * side : 0;
	return true;
}

bool TerrainData::build_section(TerrainSection &section, String &r_error, const SafeFlag *p_cancel) const {
	const int side = section_cells + 3;
	const int sample_count = side * side * side;
	const int cx = section.address.x, cy = section.address.y, cz = section.address.z;
	const Vector3 end = bounds.get_end();
	section.fields.resize(sample_count);
	section.materials.resize(sample_count);
	float *fields = section.fields.ptrw();
	uint16_t *materials = section.materials.ptrw();
	bool uniform = true;
	Vector<TerrainSample> columns;
	if (column_profile) {
		columns.resize(side * side);
		for (int z = -1; z <= section_cells + 1; ++z) {
			if (p_cancel && p_cancel->is_set()) {
				return false;
			}
			for (int x = -1; x <= section_cells + 1; ++x) {
				columns.write[(x + 1) + side * (z + 1)] = sample(Vector3((cx * section_cells + x) * spacing, bounds.get_center().y, (cz * section_cells + z) * spacing));
			}
		}
	}
	for (int z = -1; z <= section_cells + 1; ++z) {
		if (p_cancel && p_cancel->is_set()) {
			return false;
		}
		for (int y = -1; y <= section_cells + 1; ++y) {
			for (int x = -1; x <= section_cells + 1; ++x) {
				// Compute from global integer sample coordinates on every section.
				// Shared border samples therefore evaluate identically.
				const Vector3 position = Vector3(cx * section_cells + x, cy * section_cells + y, cz * section_cells + z) * spacing;
				TerrainSample value;
				if (column_profile && position.y >= bounds.position.y && position.y <= end.y) {
					value = columns[(x + 1) + side * (z + 1)];
					value.field += position.y - bounds.get_center().y;
				} else {
					value = sample(position);
				}
				if (!Math::is_finite(value.field)) {
					r_error = "A decorator produced a non-finite terrain sample.";
					return false;
				}
				// A narrow signed field: distant air/solid becomes compressible.
				value.field = CLAMP(value.field, float(-4 * spacing), float(4 * spacing));
				const int index = section.index(x, y, z);
				fields[index] = value.field;
				materials[index] = value.material;
				if (x == -1 && y == -1 && z == -1) {
					section.uniform_sample = value;
				} else if (value.field != section.uniform_sample.field || value.material != section.uniform_sample.material) {
					uniform = false;
				}
			}
		}
	}
	section.uniform = uniform;
	if (uniform) {
		section.fields.clear();
		section.materials.clear();
	}
	return true;
}

bool TerrainData::build(String &r_error) {
	if (!prepare(r_error)) {
		return false;
	}
	TerrainSection *output = sections.ptrw();
	for (int i = 0; i < sections.size(); ++i) {
		if (!build_section(output[i], r_error)) {
			return false;
		}
	}
	return true;
}

int64_t TerrainData::get_payload_bytes() const {
	int64_t bytes = 0;
	for (const TerrainSection &section : sections) {
		bytes += int64_t(section.fields.size()) * sizeof(float) + int64_t(section.materials.size()) * sizeof(uint16_t);
	}
	return bytes;
}

int TerrainData::get_uniform_section_count() const {
	int count = 0;
	for (const TerrainSection &section : sections) {
		count += section.uniform;
	}
	return count;
}

Vector3 TerrainData::sampled_normal(const TerrainSection &p_section, const Vector3 &p_position, uint16_t &r_material) const {
	const Vector3 local = (p_position - p_section.origin) / spacing;
	const Vector3i cell(CLAMP(int(Math::floor(local.x)), 0, p_section.cells - 1), CLAMP(int(Math::floor(local.y)), 0, p_section.cells - 1), CLAMP(int(Math::floor(local.z)), 0, p_section.cells - 1));
	const Vector3 fraction = (local - Vector3(cell)).clamp(Vector3(), Vector3(1, 1, 1));
	Vector3 gradient;
	real_t largest_weight = -1;
	for (int z = 0; z <= 1; ++z) {
		for (int y = 0; y <= 1; ++y) {
			for (int x = 0; x <= 1; ++x) {
				const int ix = cell.x + x, iy = cell.y + y, iz = cell.z + z;
				const real_t weight = (x ? fraction.x : 1 - fraction.x) * (y ? fraction.y : 1 - fraction.y) * (z ? fraction.z : 1 - fraction.z);
				gradient += Vector3(p_section.field_at(ix + 1, iy, iz) - p_section.field_at(ix - 1, iy, iz), p_section.field_at(ix, iy + 1, iz) - p_section.field_at(ix, iy - 1, iz), p_section.field_at(ix, iy, iz + 1) - p_section.field_at(ix, iy, iz - 1)) * weight;
				if (weight > largest_weight) {
					largest_weight = weight;
					r_material = p_section.materials[p_section.index(ix, iy, iz)];
				}
			}
		}
	}
	return gradient.length_squared() > 1e-12 ? gradient.normalized() : Vector3(0, 1, 0);
}
