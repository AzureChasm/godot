/**************************************************************************/
/*  terrain_build_job.h                                                   */
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

#include "terrain_data.h"

#include "core/os/thread.h"
#include "core/templates/local_vector.h"

// CPU-only handoff. Workers never read nodes, mutable Resources, rendering or
// physics servers. Each callback owns exactly one preallocated result slot.
struct PreparedTerrainSurface {
	Vector3 origin;
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedColorArray colors;
	PackedInt32Array indices;
	PackedVector3Array collision_faces;
};

class TerrainBuildJob {
	struct SectionResult {
		Vector<PreparedTerrainSurface> surfaces;
		String error;
		uint64_t sampling_usec = 0;
		uint64_t meshing_usec = 0;
		Thread::ID thread_id = Thread::UNASSIGNED_ID;
	};
	LocalVector<SectionResult> results;
	TerrainSection *section_output = nullptr;
	static void run(void *p_userdata);
	static void build_section(void *p_userdata, uint32_t p_index);
	void simplify_surface(TerrainSurface &r_surface, const TerrainSection &p_section, int p_level) const;
	void split_surface(const TerrainSurface &p_surface, Vector<PreparedTerrainSurface> &r_output) const;

public:
	String heightmap_csv_path;
	int heightmap_stride = 1;
	Dictionary heightmap_statistics;
	String heightmap_error;
	double heightmap_ms = 0;
	TerrainData data;
	Vector<PreparedTerrainSurface> surfaces;
	Thread coordinator;
	SafeFlag cancelled;
	SafeFlag done;
	SafeFlag main_thread_violation;
	SafeNumeric<uint32_t> completed_sections;
	SafeNumeric<uint32_t> total_sections;
	SafeNumeric<int64_t> triangles;
	bool lod_enabled = false;
	Vector3 lod_origin;
	real_t lod_distance = 16;
	real_t lod_error = 0.15;
	real_t lod_near_error = 0;
	int lod_max_level = 2;
	SafeNumeric<int64_t> source_triangles;
	SafeNumeric<uint32_t> simplified_sections;
	int workers = 1;
	int threads_used = 0;
	int mesh_section_count = 0;
	bool collision = true;
	int64_t request_id = 0;
	uint64_t started_usec = 0;
	double layout_ms = 0;
	double worker_ms = 0;
	double sampling_cpu_ms = 0;
	double meshing_cpu_ms = 0;
	String error;

	bool start();
	~TerrainBuildJob();
};
