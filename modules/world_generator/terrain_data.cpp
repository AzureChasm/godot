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

bool TerrainData::build(String &r_error) {
	if (!bounds.position.is_finite() || !bounds.size.is_finite() || bounds.size.x <= 0 || bounds.size.y <= 0 || bounds.size.z <= 0 ||
			!Math::is_finite(spacing) || spacing < 0.025 || spacing > 2 || chunk_cells < 4 || chunk_cells > 64) {
		r_error = "Invalid preview bounds, sample spacing (0.025..2), or chunk cells (4..64).";
		return false;
	}
	// Deliberate local-preview safety limit, not the production world coordinate format.
	const Vector3 end = bounds.get_end();
	if (bounds.position.abs().length() > 100000 || end.abs().length() > 100000) {
		r_error = "Prototype preview must be within 100000 metres of the origin.";
		return false;
	}
	const real_t chunk_size = spacing * chunk_cells;
	const Vector3i first(Math::floor(bounds.position.x / chunk_size), Math::floor(bounds.position.y / chunk_size), Math::floor(bounds.position.z / chunk_size));
	const Vector3i last(Math::ceil(end.x / chunk_size) - 1, Math::ceil(end.y / chunk_size) - 1, Math::ceil(end.z / chunk_size) - 1);
	const Vector3i count = last - first + Vector3i(1, 1, 1);
	const int64_t chunk_count = int64_t(count.x) * count.y * count.z;
	const int side = chunk_cells + 1;
	const int sample_count = side * side * side;
	if (chunk_count <= 0 || chunk_count > 512 || chunk_count * sample_count > 4500000) {
		r_error = "Preview budget exceeded (512 chunks / 4.5 million samples). Reduce bounds or increase sample spacing.";
		return false;
	}
	operations.sort_custom<OperationLess>();
	chunks.clear();
	chunks.reserve(chunk_count);
	for (int cz = first.z; cz <= last.z; ++cz) {
		for (int cy = first.y; cy <= last.y; ++cy) {
			for (int cx = first.x; cx <= last.x; ++cx) {
				TerrainChunk chunk;
				chunk.address = Vector3i(cx, cy, cz);
				chunk.origin = Vector3(cx, cy, cz) * chunk_size;
				chunk.cells = chunk_cells;
				chunk.fields.resize(sample_count);
				chunk.materials.resize(sample_count);
				float *fields = chunk.fields.ptrw();
				uint16_t *materials = chunk.materials.ptrw();
				bool uniform = true;
				for (int z = 0; z < side; ++z) {
					for (int y = 0; y < side; ++y) {
						for (int x = 0; x < side; ++x) {
							// Compute from global integer sample coordinates on every chunk.
							// Shared border samples therefore evaluate identically.
							const Vector3 position = Vector3(cx * chunk_cells + x, cy * chunk_cells + y, cz * chunk_cells + z) * spacing;
							TerrainSample value = sample(position);
							if (!Math::is_finite(value.field)) {
								r_error = "A decorator produced a non-finite terrain sample.";
								return false;
							}
							// A narrow signed field: distant air/solid becomes compressible.
							value.field = CLAMP(value.field, float(-4 * spacing), float(4 * spacing));
							const int index = chunk.index(x, y, z);
							fields[index] = value.field;
							materials[index] = value.material;
							if (index == 0) {
								chunk.uniform_sample = value;
							} else if (value.field != chunk.uniform_sample.field || value.material != chunk.uniform_sample.material) {
								uniform = false;
							}
						}
					}
				}
				chunk.uniform = uniform;
				if (uniform) {
					chunk.fields.clear();
					chunk.materials.clear();
				}
				chunks.push_back(chunk);
			}
		}
	}
	return true;
}

int64_t TerrainData::get_payload_bytes() const {
	int64_t bytes = 0;
	for (const TerrainChunk &chunk : chunks) {
		bytes += int64_t(chunk.fields.size()) * sizeof(float) + int64_t(chunk.materials.size()) * sizeof(uint16_t);
	}
	return bytes;
}

int TerrainData::get_uniform_chunk_count() const {
	int count = 0;
	for (const TerrainChunk &chunk : chunks) {
		count += chunk.uniform;
	}
	return count;
}
