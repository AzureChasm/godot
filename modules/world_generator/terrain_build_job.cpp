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

#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

namespace {
constexpr int TRIANGLES_PER_PUBLICATION = 1024;
}

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
	} else if (!job.cancelled.is_set() && job.data.prepare(job.error)) {
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
