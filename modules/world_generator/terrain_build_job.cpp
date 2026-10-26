/**************************************************************************/
/*  terrain_build_job.cpp                                                 */
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

#include "terrain_build_job.h"

#include "terrain_heightmap.h"

#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"
#include "scene/resources/surface_tool.h"

namespace {
constexpr int TRIANGLES_PER_PUBLICATION = 1024;
HashMap<uint64_t, int> edge_counts(const PackedInt32Array &p_indices) {
	HashMap<uint64_t, int> result;
	for (int i = 0; i < p_indices.size(); i += 3) {
		for (int edge = 0; edge < 3; ++edge) {
			const uint32_t a = p_indices[i + edge];
			const uint32_t b = p_indices[i + (edge + 1) % 3];
			const uint64_t key = (uint64_t(MIN(a, b)) << 32) | MAX(a, b);
			int *count = result.getptr(key);
			if (count) {
				++*count;
			} else {
				result.insert(key, 1);
			}
		}
	}
	return result;
}
uint32_t section_faces(const Vector3 &p_point, real_t p_size, uint32_t p_flat_axes) {
	uint32_t faces = 0;
	for (int axis = 0; axis < 3; ++axis) {
		if (p_flat_axes & (1u << axis)) {
			continue;
		}
		if (Math::abs(p_point[axis]) < 0.00001) {
			faces |= 1u << (axis * 2);
		}
		if (Math::abs(p_point[axis] - p_size) < 0.00001) {
			faces |= 1u << (axis * 2 + 1);
		}
	}
	return faces;
}
bool preserves_boundaries(const PackedInt32Array &p_source, const PackedInt32Array &p_candidate, const PackedVector3Array &p_vertices, real_t p_section_size, uint32_t p_flat_axes, uint32_t &r_changed_faces) {
	const auto faces = [&](uint64_t key) {
		return section_faces(p_vertices[int(key >> 32)], p_section_size, p_flat_axes) & section_faces(p_vertices[int(uint32_t(key))], p_section_size, p_flat_axes);
	};
	r_changed_faces = 0;
	const HashMap<uint64_t, int> source = edge_counts(p_source);
	const HashMap<uint64_t, int> candidate = edge_counts(p_candidate);
	for (const KeyValue<uint64_t, int> &edge : source) {
		const int *count = candidate.getptr(edge.key);
		if (!count || *count != edge.value) {
			if (edge.value != 2) {
				r_changed_faces = 0;
				return false;
			}
			r_changed_faces |= faces(edge.key);
		}
	}
	for (const KeyValue<uint64_t, int> &edge : candidate) {
		const int *count = source.getptr(edge.key);
		if (!count || *count != edge.value) {
			if (edge.value != 2) {
				r_changed_faces = 0;
				return false;
			}
			r_changed_faces |= faces(edge.key);
		}
	}
	return r_changed_faces == 0;
}
} //namespace

bool TerrainBuildJob::start() {
	if (!WorkerThreadPool::get_singleton() || WorkerThreadPool::get_singleton()->get_thread_count() < 1) {
		return false;
	}
	return coordinator.start(run, this) != Thread::UNASSIGNED_ID;
}

TerrainBuildJob::~TerrainBuildJob() {
	cancelled.set();
	if (coordinator.is_started()) {
		coordinator.wait_to_finish();
	}
}

void TerrainBuildJob::run(void *p_userdata) {
	TerrainBuildJob &job = *static_cast<TerrainBuildJob *>(p_userdata);
	Thread::set_name("TerrainCoordinator");
	const uint64_t started = OS::get_singleton()->get_ticks_usec();
	if (Thread::is_main_thread()) {
		job.main_thread_violation.set();
		job.error = "Terrain generation requires worker threads.";
	} else if (job.lod_enabled && !SurfaceTool::simplify_func) {
		job.error = "Distance LOD requires the meshoptimizer module. Disable LOD or enable that module.";
	} else if (!job.cancelled.is_set() && job.data.prepare(job.error)) {
		const auto near_first = [&job](const TerrainSection &a, const TerrainSection &b) {
			const Vector3 half = Vector3(1, 1, 1) * job.data.spacing * job.data.section_cells * 0.5;
			const real_t da = (a.origin + half).distance_squared_to(job.lod_origin);
			const real_t db = (b.origin + half).distance_squared_to(job.lod_origin);
			if (da == db) {
				return a.address < b.address;
			}
			return da < db;
		};
		job.data.sections.sort_custom<decltype(near_first)>(near_first);
		job.results.resize(job.data.sections.size());
		// Detach all outer COW storage before launching callbacks. No callback
		// resizes this vector or calls its non-const write accessor.
		job.section_output = job.data.sections.ptrw();
		job.total_sections.set(job.data.sections.size());
		WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
		job.workers = MIN(job.workers, pool->get_thread_count());
		const WorkerThreadPool::GroupID group = pool->add_native_group_task(build_section, &job, job.results.size(), job.workers, false, "Terrain sections");
		// This wait is on the dedicated coordinator, NEVER the main thread.
		pool->wait_for_group_task_completion(group);
		HashSet<Thread::ID> threads;
		for (const SectionResult &result : job.results) {
			if (result.thread_id != Thread::UNASSIGNED_ID) {
				threads.insert(result.thread_id);
			}
			if (!result.surfaces.is_empty()) {
				++job.mesh_section_count;
			}
			job.sampling_cpu_ms += double(result.sampling_usec) / 1000.0;
			job.meshing_cpu_ms += double(result.meshing_usec) / 1000.0;
			if (job.error.is_empty() && !result.error.is_empty()) {
				job.error = result.error;
			}
			if (!job.cancelled.is_set()) {
				job.surfaces.append_array(result.surfaces);
			}
		}
		job.threads_used = threads.size();
		if (job.triangles.get() > job.data.triangle_budget) {
			job.error = vformat("Triangle budget exceeded (%d). Increase spacing or reduce view distance.", job.data.triangle_budget);
		}
	}
	if (!job.cancelled.is_set() && job.error.is_empty() && !job.heightmap_csv_path.is_empty()) {
		const uint64_t heightmap_started = OS::get_singleton()->get_ticks_usec();
		TerrainHeightmap::write_csv(job.data, job.heightmap_csv_path, job.heightmap_stride, job.request_id, job.heightmap_statistics, job.heightmap_error, &job.cancelled);
		job.heightmap_ms = double(OS::get_singleton()->get_ticks_usec() - heightmap_started) / 1000.0;
	}
	job.worker_ms = double(OS::get_singleton()->get_ticks_usec() - started) / 1000.0;
	// Release/acquire handoff: the main thread may inspect results only after done.
	job.done.set();
}

void TerrainBuildJob::build_section(void *p_userdata, uint32_t p_index) {
	TerrainBuildJob &job = *static_cast<TerrainBuildJob *>(p_userdata);
	if (job.cancelled.is_set()) {
		return;
	}
	SectionResult &result = job.results[p_index];
	result.thread_id = Thread::get_caller_id();
	if (Thread::is_main_thread()) {
		job.main_thread_violation.set();
		result.error = "Refusing terrain generation on the main thread.";
		job.cancelled.set();
		return;
	}
	const uint64_t start = OS::get_singleton()->get_ticks_usec();
	TerrainSection &section = job.section_output[p_index];
	if (!job.data.build_section(section, result.error, &job.cancelled)) {
		if (!result.error.is_empty()) {
			job.cancelled.set();
		}
		return;
	}
	const uint64_t sampled = OS::get_singleton()->get_ticks_usec();
	result.sampling_usec = sampled - start;
	TerrainSurface surface;
	if (!TerrainMesher::build_section(job.data, section, surface, result.error, &job.cancelled)) {
		if (!result.error.is_empty()) {
			job.cancelled.set();
		}
		return;
	}
	job.source_triangles.add(surface.indices.size() / 3);
	if (job.lod_enabled && !surface.indices.is_empty() && !job.cancelled.is_set()) {
		const AABB box(section.origin, Vector3(1, 1, 1) * (section.cells * job.data.spacing));
		const Vector3 nearest = job.lod_origin.clamp(box.position, box.get_end());
		const real_t distance = nearest.distance_to(job.lod_origin);
		int level = 0;
		while (level < job.lod_max_level && distance >= job.lod_distance * (1 << level)) {
			++level;
		}
		if (level > 0 || job.lod_near_error > 0) {
			const int before = surface.indices.size();
			job.simplify_surface(surface, section, level);
			if (surface.indices.size() < before) {
				job.simplified_sections.increment();
			}
		}
	}
	const int64_t total = job.triangles.add(surface.indices.size() / 3);
	if (total > job.data.triangle_budget) {
		job.cancelled.set();
		return;
	}
	if (!job.cancelled.is_set()) {
		job.split_surface(surface, result.surfaces);
	}
	result.meshing_usec = OS::get_singleton()->get_ticks_usec() - sampled;
	job.completed_sections.increment();
}

void TerrainBuildJob::simplify_surface(TerrainSurface &r_surface, const TerrainSection &p_section, int p_level) const {
	// Keep every topological boundary fixed: adjacent independently simplified
	// sections retain the same edge vertices, even at different detail levels.
	const real_t section_size = p_section.cells * data.spacing;
	uint32_t flat_axes = 0;
	// A wholly planar section can simplify its interior; its outline remains
	// topologically locked. Mixed cliff/ground faces require the stronger guard.
	for (int axis = 0; axis < 3; ++axis) {
		bool all_min = true, all_max = true;
		for (const Vector3 &v : r_surface.vertices) {
			all_min &= Math::abs(v[axis]) < 0.00001;
			all_max &= Math::abs(v[axis] - section_size) < 0.00001;
		}
		if (all_min || all_max) {
			flat_axes |= 1u << axis;
		}
	}

	Vector<float> positions;
	positions.resize(r_surface.vertices.size() * 3);
	Vector<uint32_t> vertex_faces;
	vertex_faces.resize(r_surface.vertices.size());
	for (int i = 0; i < r_surface.vertices.size(); ++i) {
		const Vector3 &v = r_surface.vertices[i];
		for (int axis = 0; axis < 3; ++axis) {
			positions.write[i * 3 + axis] = v[axis];
		}
		vertex_faces.write[i] = section_faces(v, section_size, flat_axes);
	}
	const int target = MAX(3, (int(r_surface.indices.size()) / (1 << (2 * MAX(1, p_level)))) / 3 * 3);
	const real_t simplification_error = p_level == 0 ? lod_near_error : lod_error * (1 << (p_level - 1));
	uint32_t protected_faces = 0;
	// First try the full surface. If it changes an edge on a geometric section
	// face, retain that face's original triangle collar and simplify the interior.
	// Every retry protects at least one more face, bounding this to seven passes.
	for (int attempt = 0; attempt < 7; ++attempt) {
		PackedInt32Array fixed, interior;
		for (int i = 0; i < r_surface.indices.size(); i += 3) {
			const int a = r_surface.indices[i], b = r_surface.indices[i + 1], c = r_surface.indices[i + 2];
			const bool keep = ((vertex_faces[a] | vertex_faces[b] | vertex_faces[c]) & protected_faces) != 0;
			PackedInt32Array &out = keep ? fixed : interior;
			out.push_back(a);
			out.push_back(b);
			out.push_back(c);
		}
		if (interior.is_empty()) {
			return;
		}
		PackedInt32Array simplified;
		simplified.resize(interior.size());
		const size_t count = SurfaceTool::simplify_func(
				reinterpret_cast<unsigned int *>(simplified.ptrw()),
				reinterpret_cast<const unsigned int *>(interior.ptr()), interior.size(),
				positions.ptr(), r_surface.vertices.size(), sizeof(float) * 3, MAX(3, target - int(fixed.size())), simplification_error,
				SurfaceTool::SIMPLIFY_LOCK_BORDER | SurfaceTool::SIMPLIFY_ERROR_ABSOLUTE, nullptr);
		simplified.resize(count);
		fixed.append_array(simplified);
		uint32_t changed_faces = 0;
		if (preserves_boundaries(r_surface.indices, fixed, r_surface.vertices, section_size, flat_axes, changed_faces)) {
			r_surface.indices = fixed;
			return;
		}
		if (!changed_faces || (changed_faces & ~protected_faces) == 0) {
			return;
		}
		protected_faces |= changed_faces;
	}
}

void TerrainBuildJob::split_surface(const TerrainSurface &p_surface, Vector<PreparedTerrainSurface> &r_output) const {
	const Vector3 *vertices = p_surface.vertices.ptr();
	const Vector3 *normals = p_surface.normals.ptr();
	const Color *colors = p_surface.colors.ptr();
	const int32_t *indices = p_surface.indices.ptr();
	for (int first = 0; first < p_surface.indices.size(); first += TRIANGLES_PER_PUBLICATION * 3) {
		if (cancelled.is_set()) {
			return;
		}
		PreparedTerrainSurface piece;
		piece.origin = p_surface.origin;
		HashMap<int, int> remap;
		const int count = MIN(TRIANGLES_PER_PUBLICATION * 3, int(p_surface.indices.size()) - first);
		piece.indices.resize(count);
		int32_t *output = piece.indices.ptrw();
		if (collision) {
			piece.collision_faces.resize(count);
		}
		Vector3 *faces = collision ? piece.collision_faces.ptrw() : nullptr;
		for (int i = 0; i < count; ++i) {
			const int source = indices[first + i];
			const int *existing = remap.getptr(source);
			if (existing) {
				output[i] = *existing;
			} else {
				const int target = piece.vertices.size();
				remap.insert(source, target);
				piece.vertices.push_back(vertices[source]);
				piece.normals.push_back(normals[source]);
				piece.colors.push_back(colors[source]);
				output[i] = target;
			}
			if (faces) {
				faces[i] = vertices[source];
			}
		}
		r_output.push_back(piece);
	}
}
