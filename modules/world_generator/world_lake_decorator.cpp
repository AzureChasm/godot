/**************************************************************************/
/*  world_lake_decorator.cpp                                            */
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

#include "world_lake_decorator.h"

#include "core/object/class_db.h"
#include "core/templates/hashfuncs.h"

namespace {
real_t shoreline(real_t p_angle, real_t p_noise, int p_seed) {
	const real_t phase = double(uint32_t(p_seed) * 1664525u + 1013904223u) / 4294967296.0 * Math::TAU;
	return 1 + p_noise * (0.55 * Math::sin(p_angle * 3 + phase) + 0.3 * Math::sin(p_angle * 5 - phase) + 0.15 * Math::sin(p_angle * 9 + phase * 2));
}
class LakeModifier : public TerrainModifier {
	static SafeNumeric<uint64_t> next_cache_id;
	const uint64_t cache_id = next_cache_id.increment();

public:
	Vector3 center;
	Vector2 radius;
	real_t depth, bank_width, shore_height, shore_noise;
	int seed;
	void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const override {
		const Vector3 local = p_position - center;
		const real_t largest = MAX(radius.x, radius.y) * (1 + shore_noise) * (1 + bank_width / MIN(radius.x, radius.y));
		if (Math::abs(local.x) > largest || Math::abs(local.z) > largest) {
			return;
		}
		struct Cache {
			uint64_t id = 0;
			Vector2 point;
			real_t blend = 0;
			real_t height = 0;
		};
		static thread_local Cache cache[1024];
		const Vector2 point(local.x, local.z);
		const uint32_t hash = hash_murmur3_one_64(cache_id, hash_murmur3_one_float(point.x, hash_murmur3_one_float(point.y)));
		Cache &entry = cache[hash % 1024];
		if (entry.id != cache_id || entry.point != point) {
			const Vector2 normalized = point / radius;
			const real_t angle = normalized.angle();
			const real_t r = normalized.length() / shoreline(angle, shore_noise, seed);
			const real_t mean_radius = MIN(radius.x, radius.y);
			const real_t outer = 1 + bank_width / mean_radius;
			real_t blend = 0, height = 0;
			if (r < outer) {
				const real_t t = CLAMP((r - 1) / (outer - 1), real_t(0), real_t(1));
				blend = 1 - t * t * (3 - 2 * t);
				if (r <= 0.8) {
					height = -depth * (1 - (r / 0.8) * (r / 0.8));
				} else {
					const real_t bank = CLAMP((r - 0.8) / 0.2, real_t(0), real_t(1));
					height = shore_height * bank * bank * (3 - 2 * bank);
				}
			}
			entry = { cache_id, point, blend, height };
		}
		if (entry.blend <= 0) {
			return;
		}
		// A complete basin operation: establish its grounded bed/banks, then the
		// interior cavity. Later river/cave cuts still run after this formation.
		r_field = Math::lerp(r_field, float(local.y - entry.height), entry.blend);
		if (entry.blend > 0.5) {
			r_material = entry.height <= 0 ? 5 : 2;
		}
	}
};
SafeNumeric<uint64_t> LakeModifier::next_cache_id;
Vector<Vector3> clip(const Vector<Vector3> &p_polygon, int p_axis, real_t p_limit, bool p_greater) {
	Vector<Vector3> out;
	for (int i = 0; i < p_polygon.size(); ++i) {
		const Vector3 a = p_polygon[i], b = p_polygon[(i + 1) % p_polygon.size()];
		const bool inside_a = p_greater ? a[p_axis] >= p_limit : a[p_axis] <= p_limit;
		const bool inside_b = p_greater ? b[p_axis] >= p_limit : b[p_axis] <= p_limit;
		if (inside_a) {
			out.push_back(a);
		}
		if (inside_a != inside_b) {
			out.push_back(a.lerp(b, (p_limit - a[p_axis]) / (b[p_axis] - a[p_axis])));
		}
	}
	return out;
}
} //namespace
void WorldLakeDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("conform_water_mesh", "mesh"), &WorldLakeDecorator::conform_water_mesh);
	ClassDB::bind_method(D_METHOD("set_center", "value"), &WorldLakeDecorator::set_center);
	ClassDB::bind_method(D_METHOD("get_center"), &WorldLakeDecorator::get_center);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center"), "set_center", "get_center");
	ClassDB::bind_method(D_METHOD("set_radius", "value"), &WorldLakeDecorator::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &WorldLakeDecorator::get_radius);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "radius"), "set_radius", "get_radius");
	ClassDB::bind_method(D_METHOD("set_depth", "value"), &WorldLakeDecorator::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &WorldLakeDecorator::get_depth);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth"), "set_depth", "get_depth");
	ClassDB::bind_method(D_METHOD("set_bank_width", "value"), &WorldLakeDecorator::set_bank_width);
	ClassDB::bind_method(D_METHOD("get_bank_width"), &WorldLakeDecorator::get_bank_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bank_width"), "set_bank_width", "get_bank_width");
	ClassDB::bind_method(D_METHOD("set_shore_height", "value"), &WorldLakeDecorator::set_shore_height);
	ClassDB::bind_method(D_METHOD("get_shore_height"), &WorldLakeDecorator::get_shore_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shore_height"), "set_shore_height", "get_shore_height");
	ClassDB::bind_method(D_METHOD("set_shore_noise", "value"), &WorldLakeDecorator::set_shore_noise);
	ClassDB::bind_method(D_METHOD("get_shore_noise"), &WorldLakeDecorator::get_shore_noise);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shore_noise"), "set_shore_noise", "get_shore_noise");
	ClassDB::bind_method(D_METHOD("set_seed", "value"), &WorldLakeDecorator::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &WorldLakeDecorator::get_seed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ClassDB::bind_method(D_METHOD("create_water_mesh", "bounds"), &WorldLakeDecorator::create_water_mesh, DEFVAL(AABB()));
}
void WorldLakeDecorator::set_center(const Vector3 &p_value) {
	ERR_FAIL_COND(!p_value.is_finite() || p_value.abs().length() > 100000);
	center = p_value;
	emit_changed();
}
void WorldLakeDecorator::set_radius(const Vector2 &p_value) {
	ERR_FAIL_COND(!p_value.is_finite() || p_value.x < 2 || p_value.y < 2 || p_value.x > 4096 || p_value.y > 4096);
	radius = p_value;
	emit_changed();
}
void WorldLakeDecorator::set_depth(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 256);
	depth = p_value;
	emit_changed();
}
void WorldLakeDecorator::set_bank_width(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 1024);
	bank_width = p_value;
	emit_changed();
}
void WorldLakeDecorator::set_shore_height(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 128);
	shore_height = p_value;
	emit_changed();
}
void WorldLakeDecorator::set_shore_noise(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 0.35);
	shore_noise = p_value;
	emit_changed();
}
Ref<TerrainModifier> WorldLakeDecorator::compile(const Vector3 &p_origin, String &r_error) const {
	Ref<LakeModifier> modifier;
	modifier.instantiate();
	modifier->center = p_origin + center;
	modifier->radius = radius;
	modifier->depth = depth;
	modifier->bank_width = bank_width;
	modifier->shore_height = shore_height;
	modifier->shore_noise = shore_noise;
	modifier->seed = seed;
	return modifier;
}
Ref<ArrayMesh> WorldLakeDecorator::create_water_mesh(const AABB &p_bounds) const {
	const real_t reach = MAX(radius.x, radius.y) * (1 + shore_noise);
	if (p_bounds.has_volume() && (center.x + reach < p_bounds.position.x || center.x - reach > p_bounds.get_end().x || center.z + reach < p_bounds.position.z || center.z - reach > p_bounds.get_end().z)) {
		return Ref<ArrayMesh>();
	}
	PackedVector3Array vertices, normals;
	PackedInt32Array indices;
	constexpr int SIDES = 96;
	auto point = [&](int i) {
		const real_t angle = Math::TAU * i / SIDES;
		const real_t r = 0.8 * shoreline(angle, shore_noise, seed);
		return center + Vector3(Math::cos(angle) * radius.x * r, 0, Math::sin(angle) * radius.y * r);
	};
	for (int i = 0; i < SIDES; ++i) {
		Vector<Vector3> polygon;
		polygon.push_back(center);
		polygon.push_back(point(i));
		polygon.push_back(point(i + 1));
		if (p_bounds.has_volume()) {
			for (int axis : { 0, 2 }) {
				polygon = clip(polygon, axis, p_bounds.position[axis], true);
				polygon = clip(polygon, axis, p_bounds.get_end()[axis], false);
			}
		}
		for (int j = 1; j + 1 < polygon.size(); ++j) {
			if ((polygon[j] - polygon[0]).cross(polygon[j + 1] - polygon[0]).length_squared() < 1e-12) {
				continue;
			}
			for (const Vector3 &v : { polygon[0], polygon[j], polygon[j + 1] }) {
				indices.push_back(vertices.size());
				vertices.push_back(v);
				normals.push_back(Vector3(0, 1, 0));
			}
		}
	}
	if (indices.is_empty()) {
		return Ref<ArrayMesh>();
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

Ref<ArrayMesh> WorldLakeDecorator::conform_water_mesh(const Ref<ArrayMesh> &p_mesh) const {
	if (p_mesh.is_null()) {
		return p_mesh;
	}
	Ref<ArrayMesh> result;
	result.instantiate();
	for (int surface = 0; surface < p_mesh->get_surface_count(); ++surface) {
		Array arrays = p_mesh->surface_get_arrays(surface);
		PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		for (int i = 0; i < vertices.size(); ++i) {
			Vector3 p = vertices[i];
			const Vector2 relative = Vector2(p.x - center.x, p.z - center.z) / radius;
			if (relative.length() <= 0.8 * shoreline(relative.angle(), shore_noise, seed)) {
				p.y = center.y - 0.02; // The standing lake surface owns the visible plane.
				vertices.set(i, p);
			}
		}
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		result->add_surface_from_arrays(p_mesh->surface_get_primitive_type(surface), arrays);
		result->surface_set_material(surface, p_mesh->surface_get_material(surface));
	}
	return result;
}
