/**************************************************************************/
/*  world_region_layout.cpp                                               */
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

#include "world_region_layout.h"

#include "core/object/class_db.h"
#include "core/templates/hashfuncs.h"

namespace {
uint32_t cell_hash(int p_x, int p_z, uint32_t p_seed) {
	uint32_t hash = uint32_t(p_x) * 0x8da6b343u ^ uint32_t(p_z) * 0xd8163841u ^ p_seed;
	hash ^= hash >> 16;
	hash *= 0x7feb352du;
	hash ^= hash >> 15;
	hash *= 0x846ca68bu;
	hash ^= hash >> 16;
	return hash;
}
// Shared geometry for immutable sampling and authoring diagnostics. Sites live
// inside their own lattice cell, so a fixed 5x5 search is sufficient even at
// maximum jitter. The Voronoi transition support is capped at half a cell.
struct CellInfluence {
	Vector2i cell;
	Vector2i region;
	Vector2 center;
	real_t weight = 0;
};
struct LayoutGeometry {
	WorldRegionLayout::LayoutMode mode = WorldRegionLayout::GRID;
	real_t cell_size = 24;
	real_t transition_width = 8;
	real_t jitter = 1;
	real_t boundary_noise = 0;
	int cells_per_region = 4;
	Vector2 scale = Vector2(1, 1);
	uint32_t seed = 0;

	// Smooth seeded domain warp bends ownership and blend boundaries together.
	// Coordinates remain continuous across chunk boundaries; grid mode stays exact.
	real_t noise(Vector2 p, uint32_t salt) const {
		Vector2i cell(Math::floor(p.x), Math::floor(p.y));
		Vector2 t = p - Vector2(cell);
		t = t * t * (Vector2(3, 3) - 2 * t);
		auto value = [&](int x, int z) { return real_t(double(cell_hash(x, z, seed ^ salt)) / 2147483648.0 - 1); };
		return Math::lerp(Math::lerp(value(cell.x, cell.y), value(cell.x + 1, cell.y), t.x), Math::lerp(value(cell.x, cell.y + 1), value(cell.x + 1, cell.y + 1), t.x), t.y);
	}
	Vector2 query_point(const Vector2 &p_position) const {
		Vector2 p = p_position / (cell_size * scale);
		if (mode == WorldRegionLayout::VORONOI && boundary_noise > 0) {
			struct WarpCache {
				Vector2 input;
				Vector2 output;
				uint32_t seed = 0;
				real_t amount = -1;
			};
			static thread_local WarpCache cache[4096];
			const uint32_t hash = hash_murmur3_one_float(p.x, hash_murmur3_one_float(p.y, seed));
			WarpCache &entry = cache[hash % 4096];
			if (entry.input == p && entry.seed == seed && entry.amount == boundary_noise) {
				return entry.output;
			}
			const Vector2 input = p;
			const Vector2 q = p * 2;
			p += boundary_noise * (Vector2(noise(q, 173), noise(q, 397)) + Vector2(noise(q * 2, 571), noise(q * 2, 839)) * 0.25);
			entry = { input, p, seed, boundary_noise };
		}
		return p;
	}

	Vector2 site(const Vector2i &p_cell, uint32_t p_salt = 0) const {
		const real_t x = double(cell_hash(p_cell.x, p_cell.y, seed ^ p_salt)) / 4294967296.0;
		const real_t z = double(cell_hash(p_cell.x, p_cell.y, seed ^ p_salt ^ 0x9e3779b9u)) / 4294967296.0;
		return Vector2(p_cell.x + 0.5, p_cell.y + 0.5) + Vector2(x - 0.5, z - 0.5) * (0.8 * jitter);
	}
	Vector2i nearest(const Vector2 &p_point, uint32_t p_salt) const {
		const Vector2i base(int(Math::floor(p_point.x)), int(Math::floor(p_point.y)));
		real_t closest = 1e30;
		Vector2i result;
		for (int z = -2; z <= 2; ++z) {
			for (int x = -2; x <= 2; ++x) {
				const Vector2i candidate = base + Vector2i(x, z);
				const real_t distance = p_point.distance_squared_to(site(candidate, p_salt));
				if (distance < closest) {
					closest = distance;
					result = candidate;
				}
			}
		}
		return result;
	}
	Vector2i owner(const Vector2i &p_cell) const {
		return cells_per_region == 1 ? p_cell : nearest(site(p_cell) / Math::sqrt(real_t(cells_per_region)), 0x68bc21ebu);
	}
	Vector2 region_center(const Vector2i &p_region) const {
		if (mode == WorldRegionLayout::GRID) {
			return (Vector2(p_region) + Vector2(0.5, 0.5)) * cell_size * scale;
		}
		const Vector2 center = cells_per_region == 1 ? site(p_region) : site(p_region, 0x68bc21ebu) * Math::sqrt(real_t(cells_per_region));
		return center * cell_size * scale;
	}
	int influences(const Vector2 &p_position, CellInfluence *r_cells) const {
		const Vector2 point = query_point(p_position);
		if (mode == WorldRegionLayout::GRID) {
			const Vector2 shifted = point - Vector2(0.5, 0.5);
			const Vector2i base(int(Math::floor(shifted.x)), int(Math::floor(shifted.y)));
			const real_t width = MIN(transition_width, cell_size);
			Vector2 blend;
			for (int axis = 0; axis < 2; ++axis) {
				const real_t fraction = shifted[axis] - base[axis];
				const real_t t = width > 0 ? CLAMP((fraction - 0.5) * cell_size / width + 0.5, real_t(0), real_t(1)) : (fraction >= 0.5 ? 1 : 0);
				blend[axis] = t * t * (3 - 2 * t);
			}
			int count = 0;
			for (int z = 0; z <= 1; ++z) {
				for (int x = 0; x <= 1; ++x) {
					const real_t weight = (x ? blend.x : 1 - blend.x) * (z ? blend.y : 1 - blend.y);
					if (weight <= 0) {
						continue;
					}
					CellInfluence &cell = r_cells[count++];
					cell.cell = base + Vector2i(x, z);
					cell.region = cell.cell;
					cell.center = region_center(cell.region);
					cell.weight = weight;
				}
			}
			return count;
		}
		const Vector2i base(int(Math::floor(point.x)), int(Math::floor(point.y)));
		Vector2i candidates[25];
		real_t distances[25];
		real_t closest = 1e30;
		int winner = 0;
		for (int z = -2; z <= 2; ++z) {
			for (int x = -2; x <= 2; ++x) {
				const int index = (z + 2) * 5 + x + 2;
				candidates[index] = base + Vector2i(x, z);
				distances[index] = point.distance_to(site(candidates[index]));
				if (distances[index] < closest) {
					closest = distances[index];
					winner = index;
				}
			}
		}
		const real_t width = MIN(transition_width / cell_size, real_t(0.5));
		int count = 0;
		real_t total = 0;
		for (int i = 0; i < 25; ++i) {
			const real_t proximity = width > 0 ? MAX(real_t(0), 1 - (distances[i] - closest) / width) : (i == winner ? 1 : 0);
			if (proximity <= 0) {
				continue;
			}
			const Vector2i region = owner(candidates[i]);
			const real_t weight = proximity * proximity;
			total += weight;
			// Adjacent fine cells with the same owner form ONE logical region.
			// Sum their weights before evaluating its template, avoiding internal
			// terrain seams and repeated evaluations within a region.
			int index = 0;
			while (index < count && r_cells[index].region != region) {
				++index;
			}
			if (index == count) {
				CellInfluence &cell = r_cells[count++];
				cell.cell = candidates[i];
				cell.region = region;
				cell.center = region_center(region);
				cell.weight = 0;
			}
			r_cells[index].weight += weight;
		}
		for (int i = 0; i < count; ++i) {
			r_cells[i].weight /= total;
		}
		return count;
	}
};
struct RegionDefinition {
	Vector3 origin;
	Vector<TerrainOperation> operations;
	real_t cumulative_weight = 0;
};
struct OperationLess {
	bool operator()(const TerrainOperation &p_a, const TerrainOperation &p_b) const {
		return p_a.priority == p_b.priority ? p_a.sequence < p_b.sequence : p_a.priority < p_b.priority;
	}
};

class RegionLayoutModifier : public TerrainModifier {
	static SafeNumeric<uint64_t> next_cache_id;
	const uint64_t cache_id = next_cache_id.increment();
	struct DepthCache {
		uint64_t id = 0;
		Vector2 point;
		uint32_t seed = 0;
		int definition = -1;
		real_t depth = 0;
	};
	real_t boundary_depth(const Vector2 &p_position, int p_definition, uint32_t p_seed) const {
		// Memoize only deterministic XZ-derived geometry, independently per worker.
		// Unique compilation IDs prevent stale cache hits when modifiers are replaced.
		static thread_local DepthCache cache[4096];
		uint32_t hash = hash_murmur3_one_64(cache_id, hash_murmur3_one_float(p_position.x, hash_murmur3_one_float(p_position.y)));
		hash = hash_murmur3_one_32(p_seed ^ uint32_t(p_definition), hash);
		DepthCache &entry = cache[hash % 4096];
		if (entry.id == cache_id && entry.point == p_position && entry.seed == p_seed && entry.definition == p_definition) {
			return entry.depth;
		}
		const Vector2 point = geometry.query_point(p_position);
		const Vector2i base(Math::floor(point.x), Math::floor(point.y));
		Vector2 sites[25];
		int kinds[25];
		real_t closest = 1e30;
		int nearest = -1;
		for (int z = -2; z <= 2; ++z) {
			for (int x = -2; x <= 2; ++x) {
				const int i = (z + 2) * 5 + x + 2;
				const Vector2i cell = base + Vector2i(x, z);
				sites[i] = geometry.mode == WorldRegionLayout::GRID ? Vector2(cell) + Vector2(.5, .5) : geometry.site(cell);
				const Vector2i owner = geometry.mode == WorldRegionLayout::GRID ? cell : geometry.owner(cell);
				kinds[i] = select(owner.x, owner.y, p_seed);
				const real_t distance = point.distance_squared_to(sites[i]);
				if (kinds[i] == p_definition && distance < closest) {
					closest = distance;
					nearest = i;
				}
			}
		}
		const real_t scale = geometry.cell_size * MIN(geometry.scale.x, geometry.scale.y);
		real_t foreign = 1e30;
		for (int i = 0; i < 25; ++i) {
			if (kinds[i] != p_definition) {
				foreign = MIN(foreign, point.distance_squared_to(sites[i]));
			}
		}
		// Continuous distance-gap estimate: changing the nearest matching cell does
		// not introduce a height jump. Saturation keeps this a bounded local query.
		const real_t depth = nearest >= 0 ? CLAMP((Math::sqrt(foreign) - Math::sqrt(closest)) * scale * .5, real_t(0), scale * .35) : 0;
		entry = { cache_id, p_position, p_seed, p_definition, depth };
		return depth;
	}

	struct PlanItem {
		real_t weight = 0;
		int definition = 0;
		uint32_t seed = 0;
		Vector3 instance_origin;
		real_t depth = 0;
	};
	struct Plan {
		uint64_t id = 0;
		Vector2 point;
		uint32_t seed = 0;
		int count = 0;
		PlanItem items[25];
	};

	Plan get_plan(const Vector3 &p_position, uint32_t p_instance_seed) const {
		const Vector3 local = p_position - origin;

		static thread_local Plan cache[1024];
		const Vector2 point(local.x, local.z);
		uint32_t hash = hash_murmur3_one_64(cache_id, hash_murmur3_one_float(point.x, hash_murmur3_one_float(point.y)));
		hash = hash_murmur3_one_32(p_instance_seed, hash);
		Plan &entry = cache[hash % 1024];
		if (entry.id != cache_id || entry.point != point || entry.seed != p_instance_seed) {
			CellInfluence cells[25];
			entry.id = cache_id;
			entry.point = point;
			entry.seed = p_instance_seed;
			entry.count = geometry.influences(point, cells);
			for (int i = 0; i < entry.count; ++i) {
				const CellInfluence &cell = cells[i];
				PlanItem &item = entry.items[i];
				item.weight = cell.weight;
				item.definition = select(cell.region.x, cell.region.y, p_instance_seed);
				item.seed = cell_hash(cell.region.x, cell.region.y, geometry.seed ^ p_instance_seed);
				item.instance_origin = origin + Vector3(cell.center.x, 0, cell.center.y);
				item.depth = biome_edges ? boundary_depth(point, item.definition, p_instance_seed) : 0;
			}
		}
		// Recursive child layouts may collide with this slot. Copy the bounded
		// plan before descending, so cache replacement cannot mutate our loop.
		return entry;
	}

public:
	bool biome_edges = false;
	Vector3 origin;
	LayoutGeometry geometry;
	Vector<RegionDefinition> definitions;
	bool column_profile = true;

	bool is_column_profile() const override { return column_profile; }

	int select(int p_x, int p_z, uint32_t p_instance_seed) const {
		// Explicit unsigned arithmetic: negative cells are stable too, with no
		// mutable RNG stream or dependency on exploration/request order.
		const uint32_t hash = cell_hash(p_x, p_z, geometry.seed ^ p_instance_seed);
		const double choice = double(hash) / 4294967296.0 * definitions[definitions.size() - 1].cumulative_weight;
		for (int i = 0; i < definitions.size(); ++i) {
			if (choice < definitions[i].cumulative_weight) {
				return i;
			}
		}
		return definitions.size() - 1;
	}

	void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const override {
		TerrainEvaluationContext context;
		context.world_position = p_position;
		apply_context(p_position, r_field, r_material, context);
	}
	void apply_seeded(const Vector3 &p_position, float &r_field, uint16_t &r_material, uint32_t p_instance_seed) const override {
		TerrainEvaluationContext context;
		context.world_position = p_position;
		context.instance_seed = p_instance_seed;
		apply_context(p_position, r_field, r_material, context);
	}
	void apply_column(const Vector3 *p_positions, int p_count, float *r_fields, uint16_t *r_materials, const TerrainEvaluationContext &p_context) const override {
		const Plan plan = get_plan(p_positions[0], p_context.instance_seed);
		real_t output[MAX_COLUMN_SAMPLES] = {};
		uint16_t materials[MAX_COLUMN_SAMPLES];
		for (int j = 0; j < p_count; ++j) {
			materials[j] = r_materials[j];
		}
		real_t strongest = -1;
		for (int i = 0; i < plan.count; ++i) {
			const PlanItem &item = plan.items[i];
			const RegionDefinition &definition = definitions[item.definition];
			Vector3 positions[MAX_COLUMN_SAMPLES];
			float fields[MAX_COLUMN_SAMPLES];
			uint16_t candidate_materials[MAX_COLUMN_SAMPLES];
			for (int j = 0; j < p_count; ++j) {
				positions[j] = p_positions[j] - item.instance_origin + definition.origin;
				fields[j] = r_fields[j];
				candidate_materials[j] = r_materials[j];
			}
			TerrainEvaluationContext context = p_context;
			context.instance_seed = item.seed;
			if (biome_edges) {
				context.biome_depth = item.depth;
			}
			for (const TerrainOperation &operation : definition.operations) {
				TerrainData::apply_operation_column(operation, positions, p_count, fields, candidate_materials, context);
			}
			for (int j = 0; j < p_count; ++j) {
				output[j] += fields[j] * item.weight;
				if (item.weight > strongest) {
					materials[j] = candidate_materials[j];
				}
			}
			strongest = MAX(strongest, item.weight);
		}
		for (int j = 0; j < p_count; ++j) {
			r_fields[j] = output[j];
			r_materials[j] = materials[j];
		}
	}

	void apply_context(const Vector3 &p_position, float &r_field, uint16_t &r_material, const TerrainEvaluationContext &p_context) const override {
		const Plan plan = get_plan(p_position, p_context.instance_seed);
		real_t field = 0;
		real_t strongest = -1;
		uint16_t material = r_material;
		for (int i = 0; i < plan.count; ++i) {
			const PlanItem &item = plan.items[i];
			const RegionDefinition &definition = definitions[item.definition];
			const Vector3 query = p_position - item.instance_origin + definition.origin;
			TerrainEvaluationContext context = p_context;
			context.instance_seed = item.seed;
			if (biome_edges) {
				context.biome_depth = item.depth;
			}
			float candidate_field = r_field;
			uint16_t candidate_material = r_material;
			for (const TerrainOperation &operation : definition.operations) {
				bool inside = true;
				for (const AABB &scope : operation.scopes) {
					const Vector3 end = scope.get_end();
					inside &= query.x >= scope.position.x && query.y >= scope.position.y && query.z >= scope.position.z && query.x <= end.x && query.y <= end.y && query.z <= end.z;
				}
				if (inside) {
					operation.modifier->apply_context(query, candidate_field, candidate_material, context);
				}
			}
			field += candidate_field * item.weight;
			if (item.weight > strongest) {
				strongest = item.weight;
				material = candidate_material;
			}
		}
		r_field = field;
		r_material = material;
	}
};
SafeNumeric<uint64_t> RegionLayoutModifier::next_cache_id;
} // namespace

void WorldRegionLayout::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_boundary_noise", "value"), &WorldRegionLayout::set_boundary_noise);
	ClassDB::bind_method(D_METHOD("get_boundary_noise"), &WorldRegionLayout::get_boundary_noise);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "boundary_noise", PROPERTY_HINT_RANGE, "0,0.4,0.01"), "set_boundary_noise", "get_boundary_noise");
	ClassDB::bind_method(D_METHOD("set_biome_edges", "enabled"), &WorldRegionLayout::set_biome_edges);
	ClassDB::bind_method(D_METHOD("has_biome_edges"), &WorldRegionLayout::has_biome_edges);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "biome_edges"), "set_biome_edges", "has_biome_edges");
	ClassDB::bind_method(D_METHOD("set_layout_mode", "mode"), &WorldRegionLayout::set_layout_mode);
	ClassDB::bind_method(D_METHOD("get_layout_mode"), &WorldRegionLayout::get_layout_mode);
	ClassDB::bind_method(D_METHOD("set_cell_jitter", "jitter"), &WorldRegionLayout::set_cell_jitter);
	ClassDB::bind_method(D_METHOD("get_cell_jitter"), &WorldRegionLayout::get_cell_jitter);
	ClassDB::bind_method(D_METHOD("set_cells_per_region", "count"), &WorldRegionLayout::set_cells_per_region);
	ClassDB::bind_method(D_METHOD("get_cells_per_region"), &WorldRegionLayout::get_cells_per_region);
	ClassDB::bind_method(D_METHOD("set_distribution_scale", "scale"), &WorldRegionLayout::set_distribution_scale);
	ClassDB::bind_method(D_METHOD("get_distribution_scale"), &WorldRegionLayout::get_distribution_scale);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layout_mode", PROPERTY_HINT_ENUM, "Grid,Voronoi"), "set_layout_mode", "get_layout_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_jitter", PROPERTY_HINT_RANGE, "0,1,0.05"), "set_cell_jitter", "get_cell_jitter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cells_per_region", PROPERTY_HINT_RANGE, "1,16,1"), "set_cells_per_region", "get_cells_per_region");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "distribution_scale"), "set_distribution_scale", "get_distribution_scale");
	BIND_ENUM_CONSTANT(GRID);
	BIND_ENUM_CONSTANT(VORONOI);
	ClassDB::bind_method(D_METHOD("get_region_at", "world_position", "instance_seed"), &WorldRegionLayout::get_region_at, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_region_size", "size"), &WorldRegionLayout::set_region_size);
	ClassDB::bind_method(D_METHOD("get_region_size"), &WorldRegionLayout::get_region_size);
	ClassDB::bind_method(D_METHOD("set_transition_width", "width"), &WorldRegionLayout::set_transition_width);
	ClassDB::bind_method(D_METHOD("get_transition_width"), &WorldRegionLayout::get_transition_width);
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &WorldRegionLayout::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &WorldRegionLayout::get_seed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "region_size", PROPERTY_HINT_RANGE, "4,256,1,or_greater"), "set_region_size", "get_region_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "transition_width", PROPERTY_HINT_RANGE, "0,64,0.5,or_greater"), "set_transition_width", "get_transition_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
}
void WorldRegionLayout::set_boundary_noise(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 0.4);
	boundary_noise = p_value;
}
void WorldRegionLayout::set_layout_mode(LayoutMode p_mode) {
	ERR_FAIL_COND(p_mode != GRID && p_mode != VORONOI);
	layout_mode = p_mode;
}
WorldRegionLayout::LayoutMode WorldRegionLayout::get_layout_mode() const {
	return layout_mode;
}
void WorldRegionLayout::set_cell_jitter(real_t p_jitter) {
	ERR_FAIL_COND(!Math::is_finite(p_jitter) || p_jitter < 0 || p_jitter > 1);
	cell_jitter = p_jitter;
}
real_t WorldRegionLayout::get_cell_jitter() const {
	return cell_jitter;
}
void WorldRegionLayout::set_cells_per_region(int p_count) {
	ERR_FAIL_COND(p_count < 1 || p_count > 16);
	cells_per_region = p_count;
}
int WorldRegionLayout::get_cells_per_region() const {
	return cells_per_region;
}
void WorldRegionLayout::set_distribution_scale(const Vector2 &p_scale) {
	ERR_FAIL_COND(!p_scale.is_finite() || p_scale.x < 0.25 || p_scale.x > 4 || p_scale.y < 0.25 || p_scale.y > 4);
	distribution_scale = p_scale;
}
Vector2 WorldRegionLayout::get_distribution_scale() const {
	return distribution_scale;
}

void WorldRegionLayout::set_region_size(real_t p_size) {
	ERR_FAIL_COND(!Math::is_finite(p_size) || p_size < 4 || p_size > 4096);
	region_size = p_size;
}
real_t WorldRegionLayout::get_region_size() const {
	return region_size;
}
void WorldRegionLayout::set_transition_width(real_t p_width) {
	ERR_FAIL_COND(!Math::is_finite(p_width) || p_width < 0 || p_width > 4096);
	transition_width = p_width;
}
real_t WorldRegionLayout::get_transition_width() const {
	return transition_width;
}
void WorldRegionLayout::set_seed(int p_seed) {
	seed = p_seed;
}
int WorldRegionLayout::get_seed() const {
	return seed;
}

bool WorldRegionLayout::collect_operations(Vector<TerrainOperation> &r_operations, String &r_error, const Vector<AABB> &p_scopes, int p_depth, bool p_assigned) const {
	Vector<AABB> scopes = p_scopes;
	if (!collect_local_operations(r_operations, r_error, scopes, p_depth, p_assigned)) {
		return false;
	}
	if (get_child_count() > 128 || r_operations.size() >= 128) {
		r_error = "Layout budget exceeded: at most 128 child definitions and 128 operations per program.";
		return false;
	}
	Ref<RegionLayoutModifier> modifier;
	modifier.instantiate();
	modifier->origin = get_global_position();
	modifier->biome_edges = biome_edges;
	modifier->geometry.cell_size = region_size;
	modifier->geometry.transition_width = transition_width;
	modifier->geometry.seed = uint32_t(seed);
	modifier->geometry.mode = layout_mode;
	modifier->geometry.jitter = cell_jitter;
	modifier->geometry.boundary_noise = boundary_noise;
	modifier->geometry.cells_per_region = cells_per_region;
	modifier->geometry.scale = distribution_scale;
	real_t total_weight = 0;
	for (int i = 0; i < get_child_count(); ++i) {
		const RegionContainer *child = Object::cast_to<RegionContainer>(get_child(i));
		if (!child) {
			r_error = "WorldRegionLayout children must be RegionContainer definitions.";
			return false;
		}
		if (child->get_occurrence_weight() <= 0) {
			continue;
		}
		RegionDefinition definition;
		definition.origin = child->get_global_position();
		// The parent supplies the instance enclosure. Ignore only the immediate
		// template box; manually authored descendants retain their local scopes.
		if (!child->collect_operations(definition.operations, r_error, Vector<AABB>(), p_depth + 1, true)) {
			return false;
		}
		definition.operations.sort_custom<OperationLess>();
		for (const TerrainOperation &operation : definition.operations) {
			modifier->column_profile &= operation.modifier->is_column_profile() && operation.scopes.is_empty();
		}
		total_weight += child->get_occurrence_weight();
		definition.cumulative_weight = total_weight;
		modifier->definitions.push_back(definition);
	}
	if (modifier->definitions.is_empty()) {
		r_error = "WorldRegionLayout needs at least one child with a positive occurrence weight.";
		return false;
	}
	TerrainOperation operation;
	operation.modifier = modifier;
	operation.scopes = scopes;
	operation.priority = -100;
	operation.sequence = r_operations.size();
	r_operations.push_back(operation);
	return true;
}

Dictionary WorldRegionLayout::get_region_at(const Vector3 &p_world_position, int64_t p_instance_seed) const {
	ERR_FAIL_COND_V(!p_world_position.is_finite() || p_world_position.abs().length() > 100000, Dictionary());
	const Vector3 local = p_world_position - get_global_position();
	LayoutGeometry geometry;
	geometry.cell_size = region_size;
	geometry.transition_width = 0; // Diagnose the owning cell, not the blend mixture.
	geometry.seed = uint32_t(seed);
	geometry.mode = layout_mode;
	geometry.jitter = cell_jitter;
	geometry.boundary_noise = boundary_noise;
	geometry.cells_per_region = cells_per_region;
	geometry.scale = distribution_scale;
	CellInfluence cell[25];
	geometry.influences(Vector2(local.x, local.z), cell);
	const int x = cell[0].region.x;
	const int z = cell[0].region.y;
	real_t total = 0;
	for (int i = 0; i < get_child_count(); ++i) {
		const RegionContainer *child = Object::cast_to<RegionContainer>(get_child(i));
		if (child) {
			total += child->get_occurrence_weight();
		}
	}
	if (total <= 0) {
		return Dictionary();
	}
	const double choice = double(cell_hash(x, z, uint32_t(seed) ^ uint32_t(p_instance_seed))) / 4294967296.0 * total;
	real_t cumulative = 0;
	for (int i = 0; i < get_child_count(); ++i) {
		const RegionContainer *child = Object::cast_to<RegionContainer>(get_child(i));
		if (!child || child->get_occurrence_weight() <= 0) {
			continue;
		}
		cumulative += child->get_occurrence_weight();
		if (choice < cumulative) {
			Dictionary result;
			result["instance_seed"] = int64_t(cell_hash(x, z, uint32_t(seed) ^ uint32_t(p_instance_seed)));
			result["cell"] = Vector3i(cell[0].cell.x, 0, cell[0].cell.y);
			result["region"] = Vector3i(x, 0, z);
			result["biome"] = String(child->get_name());
			result["center"] = get_global_position() + Vector3(cell[0].center.x, 0, cell[0].center.y);
			result["size"] = region_size * distribution_scale * (layout_mode == VORONOI ? Math::sqrt(real_t(cells_per_region)) : 1.0);
			return result;
		}
	}
	return Dictionary();
}
