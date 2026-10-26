/**************************************************************************/
/*  terrain_view.h                                                        */
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

#include "region_container.h"
#include "terrain_build_job.h"
#include "terrain_data.h"

class Material;

class TerrainView : public Node3D {
	GDCLASS(TerrainView, Node3D);

	NodePath root_path = NodePath("../World");
	AABB preview_area = AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8));
	real_t sample_spacing = 0.1;
	int section_cells = 32;
	bool auto_generate = true;
	bool collision_enabled = true;
	Node3D *generated = nullptr;
	TerrainData published_data;
	bool program_snapshot_enabled = false;
	bool program_snapshot_bounded = false;
	AABB program_snapshot_area;
	Vector<TerrainOperation> program_snapshot;
	Dictionary statistics;
	String status = "Not generated";
	int64_t revision = 0;
	TerrainBuildJob *job = nullptr;
	TerrainBuildJob *pending = nullptr;
	Node3D *staging = nullptr;
	Ref<Material> preview_material;
	Vector<Node3D *> retiring;
	int publication_index = 0;
	int64_t next_request = 0;
	bool request_cancelled = false;
	bool lod_enabled = false;
	Vector3 lod_origin;
	real_t lod_distance = 16;
	real_t lod_error = 0.15;
	real_t lod_near_error = 0;
	int lod_max_level = 2;
	String heightmap_csv_path;
	int heightmap_stride = 1;
	int worker_count = 1;
	int64_t sample_budget = 16000000;
	int64_t triangle_budget = 2000000;
	real_t publication_budget_ms = 4.0;
	double publication_cpu_ms = 0;
	double max_publication_step_ms = 0;

	void process_generation();
	void start_job(TerrainBuildJob *p_job);
	void retire(Node3D *p_root);
	void cleanup_retired();
	bool publish_piece(const PreparedTerrainSurface &p_surface);
	void finish_generation();

	bool fail_generation(const String &p_error);
	Callable get_generate_action();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;
	void set_preview_area(const AABB &p_area);
	AABB get_preview_area() const;
	void set_sample_spacing(real_t p_spacing);
	real_t get_sample_spacing() const;
	void set_section_cells(int p_cells);
	int get_section_cells() const;
	void set_chunk_cells(int p_cells);
	int get_chunk_cells() const;
	void set_auto_generate(bool p_enabled);
	bool is_auto_generate() const;
	void set_collision_enabled(bool p_enabled);
	bool is_collision_enabled() const;
	void set_lod_enabled(bool p_value) { lod_enabled = p_value; }
	bool is_lod_enabled() const { return lod_enabled; }
	void set_lod_origin(const Vector3 &p_value);
	Vector3 get_lod_origin() const { return lod_origin; }
	void set_lod_distance(real_t p_value);
	real_t get_lod_distance() const { return lod_distance; }
	void set_lod_max_level(int p_value);
	int get_lod_max_level() const { return lod_max_level; }
	void set_lod_near_error(real_t p_value);
	real_t get_lod_near_error() const { return lod_near_error; }
	void set_lod_error(real_t p_value);
	real_t get_lod_error() const { return lod_error; }
	void set_worker_count(int p_count);
	int get_worker_count() const;
	void set_sample_budget(int64_t p_samples);
	int64_t get_sample_budget() const;
	void set_triangle_budget(int64_t p_triangles);
	int64_t get_triangle_budget() const;
	void set_publication_budget_ms(real_t p_milliseconds);
	real_t get_publication_budget_ms() const;
	bool is_generating() const;
	Dictionary get_progress() const;
	void cancel_generation();
	bool generate_preview();
	void set_heightmap_csv_path(const String &p_path) { heightmap_csv_path = p_path; }
	String get_heightmap_csv_path() const { return heightmap_csv_path; }
	void set_heightmap_stride(int p_stride);
	int get_heightmap_stride() const { return heightmap_stride; }
	void set_program_snapshot(const Vector<TerrainOperation> &p_operations, bool p_bounded, const AABB &p_area) {
		program_snapshot = p_operations;
		program_snapshot_bounded = p_bounded;
		program_snapshot_area = p_area;
		program_snapshot_enabled = true;
	}
	void release_sample_cache() { published_data.sections.clear(); }
	void retire_preview();
	bool has_pending_retirement() const { return !retiring.is_empty(); }
	TerrainView();
	~TerrainView();
	float sample_field(const Vector3 &p_world_position) const;
	Dictionary get_statistics() const;
	String get_status() const;
};
