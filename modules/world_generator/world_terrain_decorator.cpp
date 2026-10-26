/**************************************************************************/
/*  world_terrain_decorator.cpp                                           */
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

#include "world_terrain_decorator.h"

#include "core/object/class_db.h"
#include "core/templates/hashfuncs.h"

#include <thirdparty/misc/FastNoiseLite.h>

namespace {
uint32_t landform_hash(int p_x, int p_z, uint32_t p_seed) {
	uint32_t h = uint32_t(p_x) * 0x8da6b343u ^ uint32_t(p_z) * 0xd8163841u ^ p_seed;
	h ^= h >> 16;
	h *= 0x7feb352du;
	h ^= h >> 15;
	h *= 0x846ca68bu;
	return h ^ (h >> 16);
}
real_t unit_hash(int p_x, int p_z, uint32_t p_seed) {
	return double(landform_hash(p_x, p_z, p_seed)) / 4294967296.0;
}
struct Summit {
	Vector2 point;
	real_t height;
};
Summit summit(const Vector2i &p_cell, int p_index, uint32_t p_seed, real_t p_variation, real_t p_irregularity, real_t p_height_bias, bool p_hierarchy) {
	const uint32_t salt = p_seed + uint32_t(p_index) * 0x9e3779b9u;
	const real_t angle = p_index * Math::TAU / 5.0 + unit_hash(p_cell.x, p_cell.y, p_seed) * Math::TAU;
	const real_t radius = p_index == 0 ? 0 : 0.28;
	const Vector2 offset(Math::cos(angle) * radius, Math::sin(angle) * radius);
	const Vector2 jitter(unit_hash(p_cell.x, p_cell.y, salt) - 0.5, unit_hash(p_cell.x, p_cell.y, salt ^ 0x68bc21ebu) - 0.5);
	real_t height = Math::pow(unit_hash(p_cell.x, p_cell.y, salt ^ 0x94d049bbu), p_height_bias);
	if (p_hierarchy) {
		height = p_index == 0 ? real_t(0.78) + real_t(0.22) * height : real_t(0.18) + real_t(0.56) * height;
	}
	return { Vector2(p_cell) + Vector2(0.5, 0.5) + offset + jitter * (0.12 + 0.35 * p_irregularity), (1 - p_variation) + p_variation * height };
}
real_t ridge_height(const Vector2 &p_point, const Summit &p_a, const Summit &p_b, real_t p_width, real_t p_detail, real_t p_rounding) {
	const Vector2 edge = p_b.point - p_a.point;
	const real_t t = CLAMP((p_point - p_a.point).dot(edge) / MAX(real_t(0.0001), edge.length_squared()), real_t(0), real_t(1));
	const Vector2 projected = p_a.point + edge * t;
	const real_t distance = p_point.distance_to(projected);
	const real_t cross_section = MAX(real_t(0), 1 - distance / p_width);
	if (cross_section <= 0) {
		return 0;
	}
	const real_t saddle = 0.72 + 0.28 * Math::abs(2 * t - 1);
	const real_t ribs = 0.5 + 0.5 * Math::sin(projected.x * 43 + projected.y * 31);
	const real_t flank = 1 - p_detail * ribs * ribs * (1 - cross_section);
	if (p_rounding <= 0) {
		return Math::lerp(p_a.height, p_b.height, t) * saddle * cross_section * cross_section * flank;
	}
	const real_t round_base = MAX(real_t(0), 1 - (distance / p_width) * (distance / p_width));
	const real_t profile = Math::lerp(cross_section * cross_section, round_base * round_base, p_rounding);
	return Math::lerp(p_a.height, p_b.height, t) * saddle * profile * flank;
}
real_t mountain_graph(const Vector2 &p_point, uint32_t p_seed, int p_count, real_t p_width, real_t p_detail, real_t p_variation, real_t p_rounding, real_t p_breadth, real_t p_irregularity, real_t p_height_bias, bool p_hierarchy, uint64_t p_cache_id) {
	const Vector2i cell(Math::floor(p_point.x), Math::floor(p_point.y));
	struct TileCache {
		uint64_t id = 0;
		Vector2i cell;
		Summit points[25][6];
	};
	// Summit positions/heights are constant within a compiled graph tile. Reuse
	// them across columns without sharing mutable state between workers.
	static thread_local TileCache tiles[16];
	const uint32_t key = hash_murmur3_one_64(p_cache_id, hash_murmur3_one_32(uint32_t(cell.x), hash_murmur3_one_32(uint32_t(cell.y))));
	TileCache &tile = tiles[key % 16];
	if (tile.id != p_cache_id || tile.cell != cell) {
		for (int z = -2; z <= 2; ++z) {
			for (int x = -2; x <= 2; ++x) {
				for (int i = 0; i < p_count; ++i) {
					tile.points[(z + 2) * 5 + x + 2][i] = summit(cell + Vector2i(x, z), i, p_seed, p_variation, p_irregularity, p_height_bias, p_hierarchy);
				}
			}
		}
		tile.id = p_cache_id;
		tile.cell = cell;
	}
	real_t height = 0;
	for (int z = -1; z <= 1; ++z) {
		for (int x = -1; x <= 1; ++x) {
			const Vector2i address = cell + Vector2i(x, z);
			const Summit *peaks = tile.points[(z + 2) * 5 + x + 2];
			for (int i = 0; i < p_count; ++i) {
				const Vector2 delta = p_point - peaks[i].point;
				const real_t distance = delta.length();
				const real_t support = p_breadth;
				const real_t base = MAX(real_t(0), 1 - distance / support);
				if (base > 0) {
					const real_t angle = distance > 1e-6 ? Math::atan2(delta.y, delta.x) : 0;
					const real_t ribs = 0.5 + 0.5 * Math::sin(angle * (18 + i * 2) + peaks[i].height * 19 + distance * 1.5);
					// Grooves follow the flanks and vanish at summit and toe.
					const real_t fluting = p_detail * 4 * base * (1 - base) * ribs * ribs;
					const real_t round_base = MAX(real_t(0), 1 - (distance / support) * (distance / support));
					const real_t body = Math::lerp(Math::pow(base, real_t(1.4)), round_base * round_base, p_rounding);
					height = MAX(height, peaks[i].height * MAX(real_t(0), body - fluting));
				}
				if (i > 0) {
					height = MAX(height, ridge_height(p_point, peaks[0], peaks[i], p_width, p_detail, p_rounding));
				}
			}
			// Fixed neighbour connections are a deterministic ridge graph, independent
			// of chunks, discovery and region-template origins. Endpoints are shared.
			const Vector2i directions[4] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) };
			for (const Vector2i &direction : directions) {
				const Vector2i neighbor = address + direction;
				const real_t edge_hash = unit_hash(MIN(address.x, neighbor.x), MIN(address.y, neighbor.y), p_seed ^ (direction.x != 0 ? 0x68bc21ebu : 0x94d049bbu));
				if (edge_hash < p_irregularity * 0.35) {
					continue;
				}
				height = MAX(height, ridge_height(p_point, peaks[0], tile.points[(z + direction.y + 2) * 5 + x + direction.x + 2][0], p_width, p_detail, p_rounding));
			}
		}
	}
	return height;
}

class SurfaceProfileModifier : public TerrainModifier {
	static SafeNumeric<uint64_t> next_cache_id;
	const uint64_t cache_id = next_cache_id.increment();
	struct HeightCache {
		uint64_t id = 0;
		Vector2 point;
		real_t depth = 0;
		real_t height = 0;
	};

public:
	WorldTerrainDecorator::Profile profile = WorldTerrainDecorator::PLAINS;
	uint32_t landform_seed = 0;
	real_t detail_strength = 0.03, ridge_width = 0.18, edge_strength = 1;
	int summit_count = 3, terrace_steps = 6;
	real_t summit_variation = 0.28;
	real_t summit_rounding = 0;
	real_t summit_breadth = 0.46;
	real_t range_irregularity = 0;
	real_t summit_height_bias = 1;
	bool hierarchical_summits = false;

	bool world_space_noise = false;
	real_t interior_elevation = 0, interior_scale = 1, interior_width = 12, roughness = -1, ridge_sharpness = 2;
	Vector3 origin;
	real_t elevation = 0;
	real_t amplitude = 1;
	real_t wavelength = 5;
	int octaves = 3;
	bool ridged = false;
	bool deep_sea = false;
	uint16_t material = 2;
	WorldTerrainDecorator::BlendMode blend_mode = WorldTerrainDecorator::REPLACE_TERRAIN;
	fastnoiselite::FastNoiseLite noise;

	bool is_column_profile() const override { return true; }
	void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const override {
		TerrainEvaluationContext context;
		context.world_position = p_position;
		apply_context(p_position, r_field, r_material, context);
	}
	real_t get_height(const Vector3 &p_position, const TerrainEvaluationContext &p_context) const {
		const Vector3 local = p_position - origin;
		const Vector2 point = world_space_noise ? Vector2(p_context.world_position.x, p_context.world_position.z) : Vector2(local.x, local.z);
		static thread_local HeightCache cache[4096];
		const uint32_t key = hash_murmur3_one_64(cache_id, hash_murmur3_one_float(point.x, hash_murmur3_one_float(point.y)));
		HeightCache &cached = cache[key % 4096];
		if (cached.id != cache_id || cached.point != point || cached.depth != p_context.biome_depth) {
			real_t frequency = 1.0 / wavelength;
			real_t weight = 1;
			real_t total_weight = 0;
			real_t relief = 0;
			for (int octave = 0; octave < octaves; ++octave) {
				real_t value = noise.GetNoise(float(point.x * frequency), float(point.y * frequency));
				if (ridged) {
					// Sharpen the crest of each octave instead of merely increasing noise amplitude.
					const real_t ridge = 1.0 - Math::abs(value);
					value = ridge_sharpness == 2 ? ridge * ridge : Math::pow(ridge, ridge_sharpness);
				} else if (deep_sea) {
					value = 0.7 * value - 0.3 * Math::abs(value);
				}
				relief += value * weight;
				total_weight += weight;
				frequency *= 2;
				// Keep mountain massifs broad; high-frequency octaves should add detail,
				// not turn an entire mountain into a field of needle-like peaks.
				weight *= roughness >= 0 ? roughness : (ridged ? 0.3 : 0.5);
			}
			real_t shaped = relief / total_weight;
			if (profile >= WorldTerrainDecorator::MOUNTAIN_RANGE) {
				const Vector2 q = point / wavelength;
				const real_t broad = noise.GetNoise(float(q.x), float(q.y));
				const real_t warp = noise.GetNoise(float(q.x * 0.45 + 37), float(q.y * 0.45 - 19));
				const real_t u = CLAMP(0.5 + 0.5 * broad, real_t(0), real_t(1));
				switch (profile) {
					case WorldTerrainDecorator::MOUNTAIN_RANGE:
						shaped = mountain_graph(q, landform_seed, summit_count, ridge_width, detail_strength, summit_variation, summit_rounding, summit_breadth, range_irregularity, summit_height_bias, hierarchical_summits, cache_id) + detail_strength * shaped;
						break;
					case WorldTerrainDecorator::FOOTHILLS:
						shaped = 0.65 * broad + 0.35 * noise.GetNoise(float(q.x * 0.5 + 13), float(q.y * 0.5));
						break;
					case WorldTerrainDecorator::PLATEAU: {
						const real_t t = CLAMP((u - 0.25) / 0.5, real_t(0), real_t(1));
						shaped = t * t * (3 - 2 * t) + detail_strength * shaped;
					} break;
					case WorldTerrainDecorator::MESA: {
						const real_t t = CLAMP((u - 0.45) / 0.12, real_t(0), real_t(1));
						shaped = t * t * (3 - 2 * t) + detail_strength * shaped;
					} break;
					case WorldTerrainDecorator::DUNES:
						shaped = (Math::sin((q.x + 0.18 * warp) * Math::TAU) + 0.16 * Math::sin((q.x * 2 + 0.12 * q.y) * Math::TAU)) * 0.7;
						break;
					case WorldTerrainDecorator::BASIN: {
						const real_t t = 1 - u;
						shaped = -t * t + detail_strength * shaped;
					} break;
					case WorldTerrainDecorator::TERRACES: {
						const real_t steps = u * terrace_steps;
						const real_t level = Math::floor(steps);
						const real_t t = CLAMP((steps - level - 0.7) / 0.3, real_t(0), real_t(1));
						shaped = (level + t * t * (3 - 2 * t)) / terrace_steps + detail_strength * shaped;
					} break;
					case WorldTerrainDecorator::VALLEY: {
						const real_t path = Math::abs(Math::sin((q.x + 0.3 * warp) * Math::PI));
						const real_t cut = MAX(real_t(0), 1 - path * path / 0.25);
						shaped = -cut * cut + detail_strength * shaped;
					} break;
					case WorldTerrainDecorator::ESCARPMENT:
						shaped = Math::tanh((Math::sin(q.x * 2 + warp) + 0.25 * broad) * 5) * 0.5 + detail_strength * shaped;
						break;
					default:
						break;
				}
			}
			real_t interior = CLAMP(p_context.biome_depth / interior_width, real_t(0), real_t(1));
			interior = interior * interior * (3 - 2 * interior);
			const real_t height = elevation + interior_elevation * interior + amplitude * Math::lerp(edge_strength, interior_scale, interior) * shaped;
			cached = { cache_id, point, p_context.biome_depth, height };
		}
		return cached.height;
	}
	void apply_height(const Vector3 &p_position, real_t p_height, float &r_field, uint16_t &r_material) const {
		const float field = (p_position - origin).y - p_height;
		switch (blend_mode) {
			case WorldTerrainDecorator::REPLACE_TERRAIN:
				r_field = field;
				r_material = material;
				break;
			case WorldTerrainDecorator::ADD_SOLID:
				if (field < r_field) {
					r_field = field;
					r_material = material;
				}
				break;
			case WorldTerrainDecorator::ADD_RELIEF:
				r_field -= p_height;
				break;
			case WorldTerrainDecorator::REMOVE_SOLID:
				r_field = MAX(r_field, field);
				break;
		}
	}
	void apply_context(const Vector3 &p_position, float &r_field, uint16_t &r_material, const TerrainEvaluationContext &p_context) const override {
		apply_height(p_position, get_height(p_position, p_context), r_field, r_material);
	}
	void apply_column(const Vector3 *p_positions, int p_count, float *r_fields, uint16_t *r_materials, const TerrainEvaluationContext &p_context) const override {
		const real_t height = get_height(p_positions[0], p_context);
		for (int i = 0; i < p_count; ++i) {
			apply_height(p_positions[i], height, r_fields[i], r_materials[i]);
		}
	}
};
SafeNumeric<uint64_t> SurfaceProfileModifier::next_cache_id;
} // namespace

WorldTerrainDecorator::WorldTerrainDecorator(real_t p_elevation, real_t p_amplitude, real_t p_wavelength, int p_octaves, int p_material) {
	elevation = p_elevation;
	amplitude = p_amplitude;
	wavelength = p_wavelength;
	octaves = p_octaves;
	material_id = p_material;
	set_priority(-100);
}

void WorldTerrainDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_hierarchical_summits", "enabled"), &WorldTerrainDecorator::set_hierarchical_summits);
	ClassDB::bind_method(D_METHOD("get_hierarchical_summits"), &WorldTerrainDecorator::get_hierarchical_summits);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hierarchical_summits"), "set_hierarchical_summits", "get_hierarchical_summits");
	ClassDB::bind_method(D_METHOD("set_summit_height_bias", "value"), &WorldTerrainDecorator::set_summit_height_bias);
	ClassDB::bind_method(D_METHOD("get_summit_height_bias"), &WorldTerrainDecorator::get_summit_height_bias);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "summit_height_bias", PROPERTY_HINT_RANGE, "0.25,4,0.05"), "set_summit_height_bias", "get_summit_height_bias");
	ClassDB::bind_method(D_METHOD("set_summit_variation", "value"), &WorldTerrainDecorator::set_summit_variation);
	ClassDB::bind_method(D_METHOD("get_summit_variation"), &WorldTerrainDecorator::get_summit_variation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "summit_variation", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_summit_variation", "get_summit_variation");
	ClassDB::bind_method(D_METHOD("set_summit_rounding", "value"), &WorldTerrainDecorator::set_summit_rounding);
	ClassDB::bind_method(D_METHOD("get_summit_rounding"), &WorldTerrainDecorator::get_summit_rounding);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "summit_rounding", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_summit_rounding", "get_summit_rounding");
	ClassDB::bind_method(D_METHOD("set_summit_breadth", "value"), &WorldTerrainDecorator::set_summit_breadth);
	ClassDB::bind_method(D_METHOD("get_summit_breadth"), &WorldTerrainDecorator::get_summit_breadth);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "summit_breadth", PROPERTY_HINT_RANGE, "0.2,0.9,0.01"), "set_summit_breadth", "get_summit_breadth");
	ClassDB::bind_method(D_METHOD("set_range_irregularity", "value"), &WorldTerrainDecorator::set_range_irregularity);
	ClassDB::bind_method(D_METHOD("get_range_irregularity"), &WorldTerrainDecorator::get_range_irregularity);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "range_irregularity", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_range_irregularity", "get_range_irregularity");

	ClassDB::bind_method(D_METHOD("set_detail_strength", "value"), &WorldTerrainDecorator::set_detail_strength);
	ClassDB::bind_method(D_METHOD("get_detail_strength"), &WorldTerrainDecorator::get_detail_strength);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "detail_strength", PROPERTY_HINT_RANGE, "0,0.5,0.01"), "set_detail_strength", "get_detail_strength");
	ClassDB::bind_method(D_METHOD("set_ridge_width", "value"), &WorldTerrainDecorator::set_ridge_width);
	ClassDB::bind_method(D_METHOD("get_ridge_width"), &WorldTerrainDecorator::get_ridge_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ridge_width", PROPERTY_HINT_RANGE, "0.04,0.5,0.01"), "set_ridge_width", "get_ridge_width");
	ClassDB::bind_method(D_METHOD("set_summit_count", "value"), &WorldTerrainDecorator::set_summit_count);
	ClassDB::bind_method(D_METHOD("get_summit_count"), &WorldTerrainDecorator::get_summit_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "summit_count", PROPERTY_HINT_RANGE, "1,6,1"), "set_summit_count", "get_summit_count");
	ClassDB::bind_method(D_METHOD("set_terrace_steps", "value"), &WorldTerrainDecorator::set_terrace_steps);
	ClassDB::bind_method(D_METHOD("get_terrace_steps"), &WorldTerrainDecorator::get_terrace_steps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "terrace_steps", PROPERTY_HINT_RANGE, "2,32,1"), "set_terrace_steps", "get_terrace_steps");
	ClassDB::bind_method(D_METHOD("set_edge_strength", "value"), &WorldTerrainDecorator::set_edge_strength);
	ClassDB::bind_method(D_METHOD("get_edge_strength"), &WorldTerrainDecorator::get_edge_strength);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "edge_strength", PROPERTY_HINT_RANGE, "0,4,0.05"), "set_edge_strength", "get_edge_strength");

	ClassDB::bind_method(D_METHOD("set_world_space_noise", "value"), &WorldTerrainDecorator::set_world_space_noise);
	ClassDB::bind_method(D_METHOD("get_world_space_noise"), &WorldTerrainDecorator::get_world_space_noise);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "world_space_noise"), "set_world_space_noise", "get_world_space_noise");
	ClassDB::bind_method(D_METHOD("set_interior_elevation", "value"), &WorldTerrainDecorator::set_interior_elevation);
	ClassDB::bind_method(D_METHOD("get_interior_elevation"), &WorldTerrainDecorator::get_interior_elevation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "interior_elevation", PROPERTY_HINT_RANGE, "-64,64,0.5,or_less,or_greater"), "set_interior_elevation", "get_interior_elevation");
	ClassDB::bind_method(D_METHOD("set_interior_scale", "value"), &WorldTerrainDecorator::set_interior_scale);
	ClassDB::bind_method(D_METHOD("get_interior_scale"), &WorldTerrainDecorator::get_interior_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "interior_scale", PROPERTY_HINT_RANGE, "0,8,0.1,or_greater"), "set_interior_scale", "get_interior_scale");
	ClassDB::bind_method(D_METHOD("set_interior_width", "value"), &WorldTerrainDecorator::set_interior_width);
	ClassDB::bind_method(D_METHOD("get_interior_width"), &WorldTerrainDecorator::get_interior_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "interior_width", PROPERTY_HINT_RANGE, "0.1,128,0.5,or_greater"), "set_interior_width", "get_interior_width");
	ClassDB::bind_method(D_METHOD("set_roughness", "value"), &WorldTerrainDecorator::set_roughness);
	ClassDB::bind_method(D_METHOD("get_roughness"), &WorldTerrainDecorator::get_roughness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "roughness", PROPERTY_HINT_RANGE, "-1,0.95,0.05"), "set_roughness", "get_roughness");
	ClassDB::bind_method(D_METHOD("set_ridge_sharpness", "value"), &WorldTerrainDecorator::set_ridge_sharpness);
	ClassDB::bind_method(D_METHOD("get_ridge_sharpness"), &WorldTerrainDecorator::get_ridge_sharpness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ridge_sharpness", PROPERTY_HINT_RANGE, "1,8,0.1"), "set_ridge_sharpness", "get_ridge_sharpness");

	ClassDB::bind_method(D_METHOD("set_elevation", "elevation"), &WorldTerrainDecorator::set_elevation);
	ClassDB::bind_method(D_METHOD("get_elevation"), &WorldTerrainDecorator::get_elevation);
	ClassDB::bind_method(D_METHOD("set_amplitude", "amplitude"), &WorldTerrainDecorator::set_amplitude);
	ClassDB::bind_method(D_METHOD("get_amplitude"), &WorldTerrainDecorator::get_amplitude);
	ClassDB::bind_method(D_METHOD("set_wavelength", "wavelength"), &WorldTerrainDecorator::set_wavelength);
	ClassDB::bind_method(D_METHOD("get_wavelength"), &WorldTerrainDecorator::get_wavelength);
	ClassDB::bind_method(D_METHOD("set_octaves", "octaves"), &WorldTerrainDecorator::set_octaves);
	ClassDB::bind_method(D_METHOD("get_octaves"), &WorldTerrainDecorator::get_octaves);
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &WorldTerrainDecorator::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &WorldTerrainDecorator::get_seed);
	ClassDB::bind_method(D_METHOD("set_material_id", "material_id"), &WorldTerrainDecorator::set_material_id);
	ClassDB::bind_method(D_METHOD("get_material_id"), &WorldTerrainDecorator::get_material_id);
	ClassDB::bind_method(D_METHOD("set_blend_mode", "blend_mode"), &WorldTerrainDecorator::set_blend_mode);
	ClassDB::bind_method(D_METHOD("get_blend_mode"), &WorldTerrainDecorator::get_blend_mode);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "elevation", PROPERTY_HINT_RANGE, "-20,20,0.05,or_less,or_greater"), "set_elevation", "get_elevation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "amplitude", PROPERTY_HINT_RANGE, "0,10,0.05,or_greater"), "set_amplitude", "get_amplitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wavelength", PROPERTY_HINT_RANGE, "0.25,100,0.05,or_greater"), "set_wavelength", "get_wavelength");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "octaves", PROPERTY_HINT_RANGE, "1,6,1"), "set_octaves", "get_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_id", PROPERTY_HINT_RANGE, "1,65535,1"), "set_material_id", "get_material_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_mode", PROPERTY_HINT_ENUM, "Replace Terrain,Add Solid,Remove Solid,Add Relief"), "set_blend_mode", "get_blend_mode");
	BIND_ENUM_CONSTANT(REPLACE_TERRAIN);
	BIND_ENUM_CONSTANT(ADD_SOLID);
	BIND_ENUM_CONSTANT(REMOVE_SOLID);
	BIND_ENUM_CONSTANT(ADD_RELIEF);
}

void WorldTerrainDecorator::set_elevation(real_t p_elevation) {
	ERR_FAIL_COND(!Math::is_finite(p_elevation) || Math::abs(p_elevation) > 100000);
	elevation = p_elevation;
	emit_changed();
}
real_t WorldTerrainDecorator::get_elevation() const {
	return elevation;
}

void WorldTerrainDecorator::set_amplitude(real_t p_amplitude) {
	ERR_FAIL_COND(!Math::is_finite(p_amplitude) || p_amplitude < 0 || p_amplitude > 100000);
	amplitude = p_amplitude;
	emit_changed();
}
real_t WorldTerrainDecorator::get_amplitude() const {
	return amplitude;
}

void WorldTerrainDecorator::set_wavelength(real_t p_wavelength) {
	ERR_FAIL_COND(!Math::is_finite(p_wavelength) || p_wavelength < 0.25 || p_wavelength > 100000);
	wavelength = p_wavelength;
	emit_changed();
}
real_t WorldTerrainDecorator::get_wavelength() const {
	return wavelength;
}

void WorldTerrainDecorator::set_octaves(int p_octaves) {
	ERR_FAIL_COND(p_octaves < 1 || p_octaves > 6);
	octaves = p_octaves;
	emit_changed();
}
int WorldTerrainDecorator::get_octaves() const {
	return octaves;
}

void WorldTerrainDecorator::set_seed(int p_seed) {
	seed = p_seed;
	emit_changed();
}
int WorldTerrainDecorator::get_seed() const {
	return seed;
}

void WorldTerrainDecorator::set_material_id(int p_material_id) {
	ERR_FAIL_COND(p_material_id < 1 || p_material_id > 65535);
	material_id = p_material_id;
	emit_changed();
}
int WorldTerrainDecorator::get_material_id() const {
	return material_id;
}

void WorldTerrainDecorator::set_blend_mode(BlendMode p_mode) {
	ERR_FAIL_COND(p_mode < REPLACE_TERRAIN || p_mode > ADD_RELIEF);
	blend_mode = p_mode;
	emit_changed();
}
WorldTerrainDecorator::BlendMode WorldTerrainDecorator::get_blend_mode() const {
	return blend_mode;
}

Ref<TerrainModifier> WorldTerrainDecorator::compile(const Vector3 &p_origin, String &r_error) const {
	if (!p_origin.is_finite() || p_origin.length() > 100000) {
		r_error = "Terrain-profile container origin exceeds the prototype coordinate budget.";
		return Ref<TerrainModifier>();
	}
	Ref<SurfaceProfileModifier> result;
	result.instantiate();
	result->origin = p_origin;
	result->profile = get_profile();
	result->landform_seed = uint32_t(seed);
	result->detail_strength = detail_strength;
	result->ridge_width = ridge_width;
	result->summit_count = summit_count;
	result->terrace_steps = terrace_steps;
	result->edge_strength = edge_strength;
	result->summit_variation = summit_variation;
	result->summit_rounding = summit_rounding;
	result->summit_breadth = summit_breadth;
	result->range_irregularity = range_irregularity;
	result->summit_height_bias = summit_height_bias;
	result->hierarchical_summits = hierarchical_summits;

	result->elevation = elevation;
	result->world_space_noise = world_space_noise;
	result->interior_elevation = interior_elevation;
	result->interior_scale = interior_scale;
	result->interior_width = interior_width;
	result->roughness = roughness;
	result->ridge_sharpness = ridge_sharpness;

	result->amplitude = amplitude;
	result->wavelength = wavelength;
	result->octaves = octaves;
	result->material = material_id;
	result->blend_mode = blend_mode;
	result->ridged = get_profile() == MOUNTAINS;
	result->deep_sea = get_profile() == DEEP_SEA;
	result->noise.SetSeed(seed);
	result->noise.SetFrequency(1.0);
	result->noise.SetNoiseType(fastnoiselite::FastNoiseLite::NoiseType_OpenSimplex2S);
	return result;
}

void WorldTerrainDecorator::set_world_space_noise(bool p_value) {
	world_space_noise = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_interior_elevation(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || Math::abs(p_value) > 100000);
	interior_elevation = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_interior_scale(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 100);
	interior_scale = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_interior_width(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.1 || p_value > 100000);
	interior_width = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_roughness(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < -1 || p_value > 0.95);
	roughness = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_ridge_sharpness(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 1 || p_value > 8);
	ridge_sharpness = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_detail_strength(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 0.5);
	detail_strength = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_ridge_width(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.04 || p_value > 0.5);
	ridge_width = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_summit_count(int p_value) {
	ERR_FAIL_COND(p_value < 1 || p_value > 6);
	summit_count = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_terrace_steps(int p_value) {
	ERR_FAIL_COND(p_value < 2 || p_value > 32);
	terrace_steps = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_edge_strength(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 4);
	edge_strength = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_summit_variation(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	summit_variation = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_summit_rounding(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	summit_rounding = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_summit_breadth(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.2 || p_value > 0.9);
	summit_breadth = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_range_irregularity(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	range_irregularity = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_summit_height_bias(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.25 || p_value > 4);
	summit_height_bias = p_value;
	emit_changed();
}

void WorldTerrainDecorator::set_hierarchical_summits(bool p_enabled) {
	hierarchical_summits = p_enabled;
	emit_changed();
}
