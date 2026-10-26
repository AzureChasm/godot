/**************************************************************************/
/*  world_river_decorator.cpp                                           */
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

#include "world_river_decorator.h"

#include "core/object/class_db.h"
#include "core/templates/hashfuncs.h"

namespace {
class RiverModifier : public TerrainModifier {
	static SafeNumeric<uint64_t> next_cache_id;
	const uint64_t cache_id = next_cache_id.increment();

public:
	PackedVector3Array path;
	Vector3 origin;
	real_t width, bank_width, depth, max_cut;
	HashMap<Vector2i, Vector<int>> bins;
	real_t bin_size = 32;
	void index_segments() {
		const real_t reach = width * .5 + bank_width;
		bin_size = MAX(real_t(32), reach * 2);
		for (int i = 1; i < path.size(); ++i) {
			Vector2 a(path[i - 1].x, path[i - 1].z), b(path[i].x, path[i].z);
			Vector2i lo(((a.min(b) - Vector2(reach, reach)) / bin_size).floor()), hi(((a.max(b) + Vector2(reach, reach)) / bin_size).floor());
			for (int z = lo.y; z <= hi.y; ++z) {
				for (int x = lo.x; x <= hi.x; ++x) {
					bins[Vector2i(x, z)].push_back(i);
				}
			}
		}
	}
	void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const override {
		const Vector3 p = p_position - origin;
		struct Cache {
			uint64_t id = 0;
			Vector2 point;
			real_t nearest = 0;
			real_t water = 0;
		};
		static thread_local Cache cache[4096];
		const Vector2 point(p.x, p.z);
		const uint32_t key = hash_murmur3_one_64(cache_id, hash_murmur3_one_float(point.x, hash_murmur3_one_float(point.y)));
		Cache &entry = cache[key % 4096];
		if (entry.id != cache_id || entry.point != point) {
			real_t nearest = 1e30;
			real_t water = 0;
			const real_t reach = width * 0.5 + bank_width;
			const Vector<int> *segments = bins.getptr(Vector2i((point / bin_size).floor()));
			if (segments) {
				for (int i : *segments) {
					const Vector3 a = path[i - 1];
					const Vector3 b = path[i];
					if (p.x < MIN(a.x, b.x) - reach || p.x > MAX(a.x, b.x) + reach || p.z < MIN(a.z, b.z) - reach || p.z > MAX(a.z, b.z) + reach) {
						continue;
					}
					const Vector2 edge(b.x - a.x, b.z - a.z);
					const real_t t = CLAMP(Vector2(p.x - a.x, p.z - a.z).dot(edge) / edge.length_squared(), real_t(0), real_t(1));
					const real_t distance = Vector2(p.x - a.x, p.z - a.z).distance_to(edge * t);
					if (distance < nearest) {
						nearest = distance;
						water = Math::lerp(a.y, b.y, t);
					}
				}
			}
			entry = { cache_id, point, nearest, water };
		}
		const real_t nearest = entry.nearest;
		const real_t water = entry.water;
		const real_t reach = width * 0.5 + bank_width;
		if (nearest >= reach) {
			return;
		}
		const real_t bank = CLAMP((nearest - width * 0.5) / bank_width, real_t(0), real_t(1));
		const real_t blend = 1 - bank * bank * (3 - 2 * bank);
		const real_t channel = CLAMP(nearest / (width * 0.5), real_t(0), real_t(1));
		const real_t bed = water - depth * (1 - 0.7 * channel * channel);
		// Only remove ground: the decorator never dams an existing cave or channel.
		float carved = MAX(r_field, float(p.y - bed));
		if (max_cut > 0) {
			carved = MIN(carved, r_field + float(max_cut));
		}
		r_field = Math::lerp(r_field, carved, blend);
		if (blend > 0.5 && r_field <= 0) {
			r_material = 4;
		}
	}
};
SafeNumeric<uint64_t> RiverModifier::next_cache_id;
} //namespace
WorldRiverDecorator::WorldRiverDecorator() {
	set_priority(10);
	path.push_back(Vector3(0, 2, -128));
	path.push_back(Vector3(0, 0, 128));
}
void WorldRiverDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_cut_depth", "value"), &WorldRiverDecorator::set_max_cut_depth);
	ClassDB::bind_method(D_METHOD("get_max_cut_depth"), &WorldRiverDecorator::get_max_cut_depth);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_cut_depth", PROPERTY_HINT_RANGE, "0,256,0.5"), "set_max_cut_depth", "get_max_cut_depth");
	ClassDB::bind_method(D_METHOD("get_generated_centerline"), &WorldRiverDecorator::get_generated_centerline);
	ClassDB::bind_method(D_METHOD("set_slope", "value"), &WorldRiverDecorator::set_slope);
	ClassDB::bind_method(D_METHOD("get_slope"), &WorldRiverDecorator::get_slope);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slope", PROPERTY_HINT_RANGE, "0,0.25,0.001"), "set_slope", "get_slope");
	ClassDB::bind_method(D_METHOD("set_centerline", "value"), &WorldRiverDecorator::set_centerline);
	ClassDB::bind_method(D_METHOD("get_centerline"), &WorldRiverDecorator::get_centerline);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "centerline"), "set_centerline", "get_centerline");
	ClassDB::bind_method(D_METHOD("set_width", "value"), &WorldRiverDecorator::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &WorldRiverDecorator::get_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width"), "set_width", "get_width");
	ClassDB::bind_method(D_METHOD("set_bank_width", "value"), &WorldRiverDecorator::set_bank_width);
	ClassDB::bind_method(D_METHOD("get_bank_width"), &WorldRiverDecorator::get_bank_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bank_width"), "set_bank_width", "get_bank_width");
	ClassDB::bind_method(D_METHOD("set_depth", "value"), &WorldRiverDecorator::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &WorldRiverDecorator::get_depth);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth"), "set_depth", "get_depth");
	ClassDB::bind_method(D_METHOD("set_meander", "value"), &WorldRiverDecorator::set_meander);
	ClassDB::bind_method(D_METHOD("get_meander"), &WorldRiverDecorator::get_meander);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "meander"), "set_meander", "get_meander");
	ClassDB::bind_method(D_METHOD("set_seed", "value"), &WorldRiverDecorator::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &WorldRiverDecorator::get_seed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ClassDB::bind_method(D_METHOD("create_water_mesh", "bounds"), &WorldRiverDecorator::create_water_mesh, DEFVAL(AABB()));
}
void WorldRiverDecorator::set_max_cut_depth(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 256);
	max_cut_depth = p_value;
	emit_changed();
}
void WorldRiverDecorator::set_slope(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 0.25);
	slope = p_value;
	emit_changed();
}
void WorldRiverDecorator::set_width(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 256);
	width = p_value;
	emit_changed();
}
void WorldRiverDecorator::set_bank_width(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 256);
	bank_width = p_value;
	emit_changed();
}
void WorldRiverDecorator::set_depth(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 256);
	depth = p_value;
	emit_changed();
}
void WorldRiverDecorator::set_meander(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 256);
	meander = p_value;
	emit_changed();
}
bool WorldRiverDecorator::validate_path(String &r_error) const {
	if (path.size() < 2 || path.size() > 4096) {
		r_error = "River path needs2..4096 points.";
		return false;
	}
	real_t length = 0;
	for (int i = 0; i < path.size(); ++i) {
		if (!path[i].is_finite() || path[i].abs().length() > 100000) {
			r_error = "Invalid river point.";
			return false;
		}
		if (i > 0) {
			const real_t horizontal = Vector2(path[i].x - path[i - 1].x, path[i].z - path[i - 1].z).length();
			if (horizontal < 0.1 || path[i].y > path[i - 1].y) {
				r_error = "River path must progress horizontally and must not climb downstream.";
				return false;
			}
			length += horizontal;
		}
	}
	if (length > 16384) {
		r_error = "River preview path exceeds16384m.";
		return false;
	}
	return true;
}
PackedVector3Array WorldRiverDecorator::build_path() const {
	PackedVector3Array result;
	real_t traveled = 0;
	for (int i = 1; i < path.size(); ++i) {
		const Vector3 a = path[i - 1], b = path[i];
		const Vector3 lateral = Vector3(b.z - a.z, 0, a.x - b.x).normalized();
		const real_t segment_length = Vector2(b.x - a.x, b.z - a.z).length();
		const int steps = MAX(1, int(Math::ceil(segment_length / 8)));
		const real_t phase = double(uint32_t(seed) * 1664525u + uint32_t(i) * 1013904223u) / 4294967296.0 * Math::TAU;
		for (int step = (i == 1 ? 0 : 1); step <= steps; ++step) {
			const real_t t = real_t(step) / steps;
			const real_t bend = meander * Math::sin(t * Math::PI) * Math::sin(t * Math::TAU * 2 + phase);
			Vector3 point = a.lerp(b, t) + lateral * bend;
			point.y -= slope * (traveled + segment_length * t);
			result.push_back(point);
		}
		traveled += segment_length;
	}
	return result;
}
Ref<TerrainModifier> WorldRiverDecorator::compile(const Vector3 &p_origin, String &r_error) const {
	if (!validate_path(r_error)) {
		return Ref<TerrainModifier>();
	}
	Ref<RiverModifier> modifier;
	modifier.instantiate();
	modifier->path = build_path();
	modifier->origin = p_origin;
	modifier->width = width;
	modifier->bank_width = bank_width;
	modifier->depth = depth;
	modifier->max_cut = max_cut_depth;
	modifier->index_segments();
	return modifier;
}
Ref<ArrayMesh> WorldRiverDecorator::create_water_mesh(const AABB &p_bounds) const {
	String error;
	ERR_FAIL_COND_V_MSG(!validate_path(error), Ref<ArrayMesh>(), error);
	const PackedVector3Array points = build_path();
	PackedVector3Array vertices, normals;
	PackedVector2Array uv;
	PackedInt32Array indices;
	for (int i = 0; i < points.size(); ++i) {
		const Vector3 direction = points[MIN(i + 1, int(points.size()) - 1)] - points[MAX(0, i - 1)];
		const Vector3 lateral = Vector3(direction.z, 0, -direction.x).normalized() * width * 0.5;
		vertices.push_back(points[i] - lateral);
		vertices.push_back(points[i] + lateral);
		normals.push_back(Vector3(0, 1, 0));
		normals.push_back(Vector3(0, 1, 0));
		uv.push_back(Vector2(0, i));
		uv.push_back(Vector2(1, i));
		if (i > 0) {
			const Vector3 midpoint = (points[i] + points[i - 1]) * 0.5;
			if (p_bounds.has_volume() && (midpoint.x < p_bounds.position.x || midpoint.x >= p_bounds.get_end().x || midpoint.z < p_bounds.position.z || midpoint.z >= p_bounds.get_end().z)) {
				continue;
			}
			const int n = i * 2;
			for (int index : { n - 2, n, n - 1, n - 1, n, n + 1 }) {
				indices.push_back(index);
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
	arrays[Mesh::ARRAY_TEX_UV] = uv;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

PackedVector3Array WorldRiverDecorator::get_generated_centerline() const {
	String error;
	ERR_FAIL_COND_V_MSG(!validate_path(error), PackedVector3Array(), error);
	return build_path();
}
