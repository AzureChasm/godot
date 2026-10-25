/**************************************************************************/
/*  terrain_data.h                                                        */
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

#pragma once

#include "core/math/aabb.h"
#include "core/math/vector3i.h"
#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/safe_refcount.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

// Compiled, immutable behavior. It never reads scene nodes or mutable resources.
class TerrainModifier : public RefCounted {
public:
	virtual bool is_column_profile() const { return false; }
	virtual void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const = 0;
};

struct TerrainOperation {
	Ref<TerrainModifier> modifier;
	Vector<AABB> scopes;
	int priority = 0;
	int sequence = 0;
};

struct TerrainSample {
	float field = 0;
	uint16_t material = 1;
};

struct TerrainSection {
	Vector3i address;
	Vector3 origin;
	int cells = 0;
	bool uniform = false;
	TerrainSample uniform_sample;
	Vector<float> fields;
	Vector<uint16_t> materials;
	int index(int p_x, int p_y, int p_z) const {
		const int side = cells + 3;
		return (p_x + 1) + side * ((p_y + 1) + side * (p_z + 1));
	}
	float field_at(int p_x, int p_y, int p_z) const {
		return uniform ? uniform_sample.field : fields[index(p_x, p_y, p_z)];
	}
};

// Published by TerrainView only after the entire candidate preview succeeds.
// This prototype uses a bounded, single-detail cache, not production streaming.
class TerrainData {
public:
	static constexpr real_t WORLD_CHUNK_SIZE = 16.0;
	int64_t world_chunk_count = 0;
	int column_samples = 0;
	AABB bounds;
	real_t spacing = 0.1;
	int section_cells = 32;
	int64_t sample_budget = 16000000;
	int64_t triangle_budget = 2000000;
	bool column_profile = false;
	Vector<TerrainOperation> operations;
	Vector<TerrainSection> sections;

	TerrainSample sample(const Vector3 &p_position) const;
	Vector3 normal(const Vector3 &p_position) const;
	Vector3 sampled_normal(const TerrainSection &p_section, const Vector3 &p_position, uint16_t &r_material) const;
	bool prepare(String &r_error);
	bool build_section(TerrainSection &r_section, String &r_error, const SafeFlag *p_cancel = nullptr) const;
	bool build(String &r_error);
	int64_t get_payload_bytes() const;
	int get_uniform_section_count() const;
};

struct TerrainSurface {
	Vector3 origin;
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedColorArray colors;
	PackedInt32Array indices;
	HashMap<Vector3, int> vertex_lookup;
};

class TerrainMesher {
public:
	static bool build_section(const TerrainData &p_data, const TerrainSection &p_section, TerrainSurface &r_surface, String &r_error, const SafeFlag *p_cancel = nullptr);
	static bool build(const TerrainData &p_data, Vector<TerrainSurface> &r_surfaces, String &r_error);
};
