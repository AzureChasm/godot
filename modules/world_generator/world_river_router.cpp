/**************************************************************************/
/*  world_river_router.cpp                                                */
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

#include "world_river_router.h"

#include "core/math/delaunay_2d.h"
#include "core/object/class_db.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

namespace {
uint32_t river_hash(int x, int z, uint32_t seed) {
	uint32_t v = uint32_t(x) * 0x8da6b343u ^ uint32_t(z) * 0xd8163841u ^ seed;
	v ^= v >> 16;
	v *= 0x7feb352du;
	v ^= v >> 15;
	v *= 0x846ca68bu;
	return v ^ (v >> 16);
}
real_t unit(int x, int z, uint32_t seed) {
	return double(river_hash(x, z, seed)) / 4294967296.0;
}
struct DrainNode {
	Vector2 point;
	real_t height = 0;
	bool valid = false, ocean = false;
	Vector<int> edges;
	real_t cost = 1e30;
	int parent = -1, parent_edge = -1;
};
struct DrainEdge {
	int a, b;
	Vector2 site_a, site_b;
	real_t barrier = 0;
	bool valid = false;
};
struct QueueEntry {
	real_t cost;
	int node;
};
class DrainHeap {
	Vector<QueueEntry> entries;
	static bool less(const QueueEntry &a, const QueueEntry &b) { return a.cost == b.cost ? a.node < b.node : a.cost < b.cost; }

public:
	bool empty() const { return entries.is_empty(); }
	void push(QueueEntry value) {
		entries.push_back(value);
		int i = entries.size() - 1;
		while (i > 0) {
			int parent = (i - 1) / 2;
			if (!less(entries[i], entries[parent])) {
				break;
			}
			SWAP(entries.write[i], entries.write[parent]);
			i = parent;
		}
	}
	QueueEntry pop() {
		QueueEntry result = entries[0];
		entries.write[0] = entries[entries.size() - 1];
		entries.resize(entries.size() - 1);
		int i = 0;
		while (i * 2 + 1 < entries.size()) {
			int child = i * 2 + 1;
			if (child + 1 < entries.size() && less(entries[child + 1], entries[child])) {
				++child;
			}
			if (!less(entries[child], entries[i])) {
				break;
			}
			SWAP(entries.write[child], entries.write[i]);
			i = child;
		}
		return result;
	}
};
} //namespace
struct WorldRiverRouter::Job {
	TerrainData terrain;
	AABB area;
	int seed = 2026, max_rivers = 6;
	real_t spacing = 48, source_height = 64, max_incision = 6, separation = 192, noise = 0.18, minimum_grade = 0.0005;
	Thread thread;
	SafeFlag cancelled, done;
	SafeNumeric<uint32_t> samples_done, samples_total;
	SafeNumeric<int> phase;
	Vector<DrainNode> nodes;
	Vector<DrainEdge> edges;
	DrainNode *node_output = nullptr;
	DrainEdge *edge_output = nullptr;
	Vector<PackedVector3Array> paths;
	PackedVector3Array lakes;
	bool coastal_lake = false, highland_lake = false;
	int ocean_nodes = 0, rejected = 0;
	double elapsed_ms = 0;
	String error;
	~Job() {
		cancelled.set();
		if (thread.is_started()) {
			thread.wait_to_finish();
		}
	}
	real_t height(Vector2 point, bool &valid) const {
		constexpr int COUNT = 65;
		Vector3 positions[COUNT];
		float fields[COUNT];
		uint16_t materials[COUNT];
		for (int i = 0; i < COUNT; ++i) {
			positions[i] = Vector3(point.x, Math::lerp(area.position.y, area.get_end().y, real_t(i) / (COUNT - 1)), point.y);
		}
		terrain.sample_column(positions, COUNT, fields, materials);
		valid = false;
		if (fields[COUNT - 1] <= 0) {
			return area.get_end().y;
		}
		for (int i = COUNT - 2; i >= 0; --i) {
			if (fields[i] <= 0) {
				real_t low = positions[i].y, high = positions[i + 1].y;
				for (int j = 0; j < 10; ++j) {
					real_t mid = (low + high) * 0.5;
					if (terrain.sample(Vector3(point.x, mid, point.y)).field <= 0) {
						low = mid;
					} else {
						high = mid;
					}
				}
				valid = true;
				return (low + high) * 0.5;
			}
		}
		return area.position.y;
	}
	static void sample(void *userdata, uint32_t index) {
		Job &j = *static_cast<Job *>(userdata);
		if (j.cancelled.is_set()) {
			return;
		}
		if (index < uint32_t(j.nodes.size())) {
			DrainNode &node = j.node_output[index];
			node.height = j.height(node.point, node.valid);
		} else {
			DrainEdge &edge = j.edge_output[index - j.nodes.size()];
			const Vector2 midpoint = (j.node_output[edge.a].point + j.node_output[edge.b].point) * 0.5;
			edge.barrier = j.height(midpoint, edge.valid);
		}
		j.samples_done.increment();
	}
	Vector2 edge_point(const DrainEdge &edge, int from, real_t t, real_t amount) const {
		real_t canonical = from == edge.a ? t : 1 - t;
		const Vector2 a = nodes[edge.a].point, b = nodes[edge.b].point;
		const real_t corridor = MIN(spacing * 0.15, a.distance_to(b) * 0.25);
		const real_t edge_phase = unit(edge.a, edge.b, uint32_t(seed)) * Math::TAU;
		const real_t offset = Math::sin(canonical * Math::PI) * Math::sin(canonical * Math::TAU + edge_phase) * corridor * amount;
		return a.lerp(b, canonical) + (b - a).orthogonal().normalized() * offset;
	}
	void generate_graph() {
		Vector<Vector2> sites;
		const int x0 = int(Math::floor(area.position.x / spacing)) - 2, x1 = int(Math::ceil(area.get_end().x / spacing)) + 2;
		const int z0 = int(Math::floor(area.position.z / spacing)) - 2, z1 = int(Math::ceil(area.get_end().z / spacing)) + 2;
		for (int z = z0; z <= z1; ++z) {
			for (int x = x0; x <= x1; ++x) {
				sites.push_back((Vector2(x, z) + Vector2(0.5, 0.5) + Vector2(unit(x, z, uint32_t(seed)) - .5, unit(x, z, uint32_t(seed) ^ 91) - .5) * 0.65) * spacing);
			}
		}
		const Vector<Delaunay2D::Triangle> triangles = Delaunay2D::triangulate(sites);
		Vector<int> indices;
		indices.resize(triangles.size());
		indices.fill(-1);
		for (int i = 0; i < triangles.size(); ++i) {
			Vector2 p = triangles[i].circum_center;
			const Vector2 centroid = (sites[triangles[i].points[0]] + sites[triangles[i].points[1]] + sites[triangles[i].points[2]]) / 3;
			// Extreme circumcenters near the outer hull are not drainage vertices.
			if (!p.is_finite() || p.distance_to(centroid) > spacing) {
				continue;
			}
			if (p.x < area.position.x || p.x > area.get_end().x || p.y < area.position.z || p.y > area.get_end().z) {
				continue;
			}
			indices.write[i] = nodes.size();
			DrainNode node;
			node.point = p;
			nodes.push_back(node);
		}
		HashMap<uint64_t, int> owner;
		for (int i = 0; i < triangles.size(); ++i) {
			for (int k = 0; k < 3; ++k) {
				int a = triangles[i].points[k], b = triangles[i].points[(k + 1) % 3];
				uint64_t key = (uint64_t(MIN(a, b)) << 32) | uint32_t(MAX(a, b));
				const int *other = owner.getptr(key);
				if (other) {
					const int na = indices[i], nb = indices[*other];
					if (na >= 0 && nb >= 0 && nodes[na].point.distance_to(nodes[nb].point) > 0.01) {
						DrainEdge edge;
						edge.a = MIN(na, nb);
						edge.b = MAX(na, nb);
						edge.site_a = sites[a];
						edge.site_b = sites[b];
						const int index = edges.size();
						edges.push_back(edge);
						nodes.write[na].edges.push_back(index);
						nodes.write[nb].edges.push_back(index);
					}
				} else {
					owner.insert(key, i);
				}
			}
		}
	}
	void route() {
		Vector<int> flood;
		for (int i = 0; i < nodes.size(); ++i) {
			const Vector2 p = nodes[i].point;
			bool border = MIN(MIN(p.x - area.position.x, area.get_end().x - p.x), MIN(p.y - area.position.z, area.get_end().z - p.y)) < spacing * 1.5;
			if (nodes[i].valid && nodes[i].height < -2 && border) {
				nodes.write[i].ocean = true;
				flood.push_back(i);
			}
		}
		for (int index = 0; index < flood.size(); ++index) {
			const int node = flood[index];
			for (int e : nodes[node].edges) {
				const int next = edges[e].a == node ? edges[e].b : edges[e].a;
				if (!nodes[next].ocean && nodes[next].valid && nodes[next].height <= 0 && edges[e].valid && edges[e].barrier <= 0) {
					nodes.write[next].ocean = true;
					flood.push_back(next);
				}
			}
		}
		ocean_nodes = flood.size();
		if (!ocean_nodes) {
			error = "No boundary-connected ocean outlet in the fixed drainage area.";
		}
		DrainHeap heap;
		for (int index : flood) {
			nodes.write[index].cost = 0;
			heap.push({ 0, index });
		}
		while (!heap.empty() && !cancelled.is_set()) {
			const QueueEntry current = heap.pop();
			if (current.cost != nodes[current.node].cost) {
				continue;
			}
			for (int e : nodes[current.node].edges) {
				const DrainEdge &edge = edges[e];
				const int next = edge.a == current.node ? edge.b : edge.a;
				if (!nodes[next].valid || !edge.valid) {
					continue;
				}
				const real_t length = nodes[next].point.distance_to(nodes[current.node].point);
				const real_t uphill = MAX(real_t(0), nodes[current.node].height - nodes[next].height);
				const real_t barrier = MAX(real_t(0), edge.barrier - MAX(nodes[next].height, nodes[current.node].height));
				const real_t cost = current.cost + length * (1 + uphill * uphill * 3 + barrier * barrier * 2);
				if (cost < nodes[next].cost) {
					nodes.write[next].cost = cost;
					nodes.write[next].parent = current.node;
					nodes.write[next].parent_edge = e;
					heap.push({ cost, next });
				}
			}
		}
		Vector<int> sources;
		for (int i = 0; i < nodes.size(); ++i) {
			if (nodes[i].valid && nodes[i].height >= source_height && nodes[i].parent >= 0) {
				sources.push_back(i);
			}
		}
		// Prefer useful central headwaters without making routes camera-dependent.
		const Vector2 study_center(area.get_center().x, area.get_center().z);
		const auto higher = [&](int a, int b) {
			const real_t score_a = nodes[a].height / (1 + nodes[a].point.distance_to(study_center) / 384);
			const real_t score_b = nodes[b].height / (1 + nodes[b].point.distance_to(study_center) / 384);
			return score_a == score_b ? a < b : score_a > score_b;
		};
		sources.sort_custom<decltype(higher)>(higher);
		Vector<Vector2> accepted_sources, tested_sources;
		HashMap<int, real_t> known_water;
		int attempts = 0;
		for (int source : sources) {
			if (cancelled.is_set() || paths.size() >= max_rivers || attempts >= 128) {
				break;
			}
			bool close = false;
			for (const Vector2 &p : accepted_sources) {
				close |= p.distance_to(nodes[source].point) < separation;
			}
			for (const Vector2 &p : tested_sources) {
				close |= p.distance_to(nodes[source].point) < separation * .4;
			}
			if (close) {
				continue;
			}
			++attempts;
			tested_sources.push_back(nodes[source].point);
			Vector<int> chain;
			int node = source;
			for (int step = 0; step < nodes.size() && node >= 0; ++step) {
				chain.push_back(node);
				if (nodes[node].ocean || known_water.has(node)) {
					break;
				}
				node = nodes[node].parent;
			}
			if (chain.size() < 3 || (!nodes[chain[chain.size() - 1]].ocean && !known_water.has(chain[chain.size() - 1]))) {
				continue;
			}
			bool accepted = false;
			for (int attempt = 0; attempt < 2 && !accepted; ++attempt) {
				PackedVector3Array path;
				Vector<real_t> water_at_nodes;
				real_t water = MAX(real_t(0), nodes[source].height - 0.75);
				path.push_back(Vector3(nodes[source].point.x, water, nodes[source].point.y));
				water_at_nodes.push_back(water);
				real_t length = 0;
				bool valid = true;
				for (int c = 1; c < chain.size() && valid; ++c) {
					const int from = chain[c - 1], to = chain[c];
					const DrainEdge &edge = edges[nodes[from].parent_edge];
					const int steps = MAX(1, int(Math::ceil(nodes[from].point.distance_to(nodes[to].point) / 2)));
					for (int step = 1; step <= steps; ++step) {
						if (cancelled.is_set()) {
							valid = false;
							break;
						}
						const Vector2 p = edge_point(edge, from, real_t(step) / steps, attempt == 0 ? noise : 0);
						bool found;
						const real_t h = height(p, found);
						const Vector3 previous = path[path.size() - 1];
						const real_t distance = Vector2(previous.x, previous.z).distance_to(p);
						if (distance < 0.1) {
							continue;
						}
						water = MAX(real_t(0), MIN(water - distance * minimum_grade, h - 0.75));
						if (c == chain.size() - 1 && step == steps && known_water.has(to)) {
							const real_t target = known_water[to];
							if (target > previous.y + 0.0001) {
								valid = false;
								break;
							}
							water = target;
						}
						if (!found || h - water > max_incision) {
							valid = false;
							break;
						}
						length += distance;
						path.push_back(Vector3(p.x, water, p.y));
					}
					water_at_nodes.push_back(water);
				}
				if (valid && length >= 128 && path.size() >= 3) {
					if (nodes[chain[chain.size() - 1]].ocean) {
						Vector3 end = path[path.size() - 1];
						end.y = 0;
						path.set(path.size() - 1, end);
					}
					if (path.size() <= 4096) {
						paths.push_back(path);
						accepted_sources.push_back(nodes[source].point);
						accepted = true;
						for (int c = 0; c < chain.size(); ++c) {
							known_water.insert(chain[c], water_at_nodes[c]);
						}
					}
				}
			}
			if (!accepted) {
				++rejected;
			}
		}
		// Suggestions are chosen from the unmodified terrain, not fixed Y constants.
		real_t coastal_score = 1e30, high_score = 1e30;
		Vector3 coastal, high;
		bool have_coast = false, have_high = false;
		const Vector2 center(area.get_center().x, area.get_center().z);
		for (int i = 0; i < nodes.size(); ++i) {
			const DrainNode &n = nodes[i];
			if (!n.valid || n.ocean) {
				continue;
			}
			real_t ocean_distance = 1e30;
			for (int o : flood) {
				ocean_distance = MIN(ocean_distance, n.point.distance_to(nodes[o].point));
			}
			bool near_river = false;
			for (const PackedVector3Array &path : paths) {
				for (const Vector3 &p : path) {
					if (n.point.distance_to(Vector2(p.x, p.z)) < 128) {
						near_river = true;
						break;
					}
				}
				if (near_river) {
					break;
				}
			}
			if (near_river) {
				continue;
			}
			const real_t distance = n.point.distance_to(center);
			if (n.height >= 2 && n.height <= 16 && ocean_distance < 192) {
				const real_t score = distance + n.height * 8;
				if (score < coastal_score) {
					coastal_score = score;
					coastal = Vector3(n.point.x, 0, n.point.y);
					have_coast = true;
				}
			}
			if (n.height >= 48 && n.height <= 150 && ocean_distance > 192) {
				real_t variation = 0;
				for (int e : n.edges) {
					int other = edges[e].a == i ? edges[e].b : edges[e].a;
					variation = MAX(variation, Math::abs(n.height - nodes[other].height));
				}
				const real_t score = distance + variation * 8;
				if (score < high_score) {
					high_score = score;
					high = Vector3(n.point.x, n.height - 1, n.point.y);
					have_high = true;
				}
			}
		}
		coastal_lake = have_coast;
		highland_lake = have_high;
		if (have_coast) {
			lakes.push_back(coastal);
		}
		if (have_high) {
			lakes.push_back(high);
		}
	}
	static void run(void *userdata) {
		Job &j = *static_cast<Job *>(userdata);
		Thread::set_name("DrainagePlanner");
		const uint64_t start = OS::get_singleton()->get_ticks_usec();
		j.phase.set(1);
		j.generate_graph();
		if (!j.cancelled.is_set()) {
			j.node_output = j.nodes.ptrw();
			j.edge_output = j.edges.ptrw();
			j.samples_total.set(j.nodes.size() + j.edges.size());
			j.phase.set(2);
			WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
			const auto group = pool->add_native_group_task(sample, &j, j.nodes.size() + j.edges.size(), MIN(OS::get_singleton()->get_processor_count(), pool->get_thread_count()), false, "Drainage height projection");
			pool->wait_for_group_task_completion(group);
			if (!j.cancelled.is_set()) {
				j.phase.set(3);
				j.route();
			}
		}
		j.elapsed_ms = double(OS::get_singleton()->get_ticks_usec() - start) / 1000;
		j.phase.set(4);
		j.done.set();
	}
};
WorldRiverRouter::~WorldRiverRouter() {
	if (job) {
		memdelete(job);
	}
}
void WorldRiverRouter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_minimum_grade", "value"), &WorldRiverRouter::set_minimum_grade);
	ClassDB::bind_method(D_METHOD("get_minimum_grade"), &WorldRiverRouter::get_minimum_grade);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "minimum_grade"), "set_minimum_grade", "get_minimum_grade");
	ClassDB::bind_method(D_METHOD("begin", "root", "area", "seed", "spacing", "max_rivers"), &WorldRiverRouter::begin, DEFVAL(2026), DEFVAL(48.0), DEFVAL(6));
	ClassDB::bind_method(D_METHOD("cancel"), &WorldRiverRouter::cancel);
	ClassDB::bind_method(D_METHOD("is_running"), &WorldRiverRouter::is_running);
	ClassDB::bind_method(D_METHOD("get_statistics"), &WorldRiverRouter::get_statistics);
	ClassDB::bind_method(D_METHOD("get_paths"), &WorldRiverRouter::get_paths);
	ClassDB::bind_method(D_METHOD("get_lakes"), &WorldRiverRouter::get_lakes);
	ClassDB::bind_method(D_METHOD("set_minimum_source_height", "value"), &WorldRiverRouter::set_minimum_source_height);
	ClassDB::bind_method(D_METHOD("get_minimum_source_height"), &WorldRiverRouter::get_minimum_source_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "minimum_source_height"), "set_minimum_source_height", "get_minimum_source_height");
	ClassDB::bind_method(D_METHOD("set_max_incision", "value"), &WorldRiverRouter::set_max_incision);
	ClassDB::bind_method(D_METHOD("get_max_incision"), &WorldRiverRouter::get_max_incision);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_incision"), "set_max_incision", "get_max_incision");
	ClassDB::bind_method(D_METHOD("set_source_separation", "value"), &WorldRiverRouter::set_source_separation);
	ClassDB::bind_method(D_METHOD("get_source_separation"), &WorldRiverRouter::get_source_separation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "source_separation"), "set_source_separation", "get_source_separation");
	ClassDB::bind_method(D_METHOD("set_edge_noise", "value"), &WorldRiverRouter::set_edge_noise);
	ClassDB::bind_method(D_METHOD("get_edge_noise"), &WorldRiverRouter::get_edge_noise);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "edge_noise"), "set_edge_noise", "get_edge_noise");
}
void WorldRiverRouter::set_minimum_source_height(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 1 || p_value > 512);
	minimum_source_height = p_value;
}
void WorldRiverRouter::set_max_incision(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 1 || p_value > 32);
	max_incision = p_value;
}
void WorldRiverRouter::set_source_separation(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 32 || p_value > 2048);
	source_separation = p_value;
}
void WorldRiverRouter::set_edge_noise(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	edge_noise = p_value;
}
void WorldRiverRouter::set_minimum_grade(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 0.02);
	minimum_grade = p_value;
}
bool WorldRiverRouter::begin(RegionContainer *p_root, const AABB &p_area, int p_seed, real_t p_spacing, int p_max_rivers) {
	ERR_FAIL_COND_V(!Thread::is_main_thread(), false);
	last_error = String();
	if (is_running()) {
		last_error = "Wait for cancellation before starting a new drainage plan.";
		return false;
	}
	if (!p_root || !p_root->is_inside_tree() || !p_area.position.is_finite() || !p_area.size.is_finite() || !p_area.has_volume() || p_area.position.abs().length() > 100000 || p_area.get_end().abs().length() > 100000 || !Math::is_finite(p_spacing) || p_spacing < 24 || p_spacing > 128 || p_max_rivers < 1 || p_max_rivers > 16 || p_area.size.x * p_area.size.z / (p_spacing * p_spacing) > 8192) {
		last_error = "Invalid drainage root, area within100000m, spacing24..128, river count1..16 or graph budget8192 sites.";
		return false;
	}
	if (job) {
		memdelete(job);
	}
	job = memnew(Job);
	job->area = p_area;
	job->terrain.bounds = p_area;
	if (p_root->is_bounded()) {
		job->area = job->area.intersection(AABB(p_root->get_area().position + p_root->get_global_position(), p_root->get_area().size));
	}
	if (!job->area.has_volume()) {
		last_error = "Drainage area does not intersect the bounded root.";
		job->done.set();
		return false;
	}
	job->seed = p_seed;
	job->spacing = p_spacing;
	job->max_rivers = p_max_rivers;
	job->source_height = minimum_source_height;
	job->max_incision = max_incision;
	job->separation = source_separation;
	job->noise = edge_noise;
	job->minimum_grade = minimum_grade;
	if (!p_root->collect_operations(job->terrain.operations, last_error)) {
		job->done.set();
		return false;
	}
	const auto order = [](const TerrainOperation &a, const TerrainOperation &b) { return a.priority == b.priority ? a.sequence < b.sequence : a.priority < b.priority; };
	job->terrain.operations.sort_custom<decltype(order)>(order);
	if (!WorkerThreadPool::get_singleton() || job->thread.start(Job::run, job) == Thread::UNASSIGNED_ID) {
		last_error = "Could not start drainage worker.";
		job->done.set();
		return false;
	}
	return true;
}
void WorldRiverRouter::cancel() {
	if (job) {
		job->cancelled.set();
	}
}
bool WorldRiverRouter::is_running() const {
	return job && !job->done.is_set();
}
Dictionary WorldRiverRouter::get_statistics() const {
	Dictionary result;
	result["running"] = is_running();
	result["phase"] = job ? job->phase.get() : 0;
	result["error"] = last_error;
	result["samples_done"] = job ? int64_t(job->samples_done.get()) : 0;
	result["samples_total"] = job ? int64_t(job->samples_total.get()) : 0;
	if (job && job->done.is_set()) {
		if (last_error.is_empty()) {
			result["error"] = job->error;
		}
		result["vertices"] = job->nodes.size();
		result["edges"] = job->edges.size();
		result["ocean_vertices"] = job->ocean_nodes;
		result["rivers"] = job->paths.size();
		result["rejected_sources"] = job->rejected;
		result["planning_ms"] = job->elapsed_ms;
		result["cancelled"] = job->cancelled.is_set();
		result["area"] = job->area;
	}
	return result;
}
Array WorldRiverRouter::get_paths() const {
	Array result;
	if (job && job->done.is_set() && !job->cancelled.is_set()) {
		for (const PackedVector3Array &path : job->paths) {
			result.push_back(path);
		}
	}
	return result;
}
Dictionary WorldRiverRouter::get_lakes() const {
	Dictionary result;
	if (job && job->done.is_set() && !job->cancelled.is_set()) {
		if (job->coastal_lake) {
			result["coastal"] = job->lakes[0];
		}
		if (job->highland_lake) {
			result["highland"] = job->lakes[job->coastal_lake ? 1 : 0];
		}
	}
	return result;
}
