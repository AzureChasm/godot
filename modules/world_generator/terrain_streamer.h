/**************************************************************************/
/*  terrain_streamer.h                                                  */
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

#include "terrain_view.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

// Residency and priority policy; TerrainView still owns each worker handoff.
class TerrainStreamer : public Node3D {
	GDCLASS(TerrainStreamer, Node3D);
	NodePath template_path;
	HashMap<Vector2i, TerrainView *> resident;
	HashSet<Vector2i> wanted;
	Vector<Vector2i> queue;
	Vector<TerrainView *> retiring;
	TerrainView *active = nullptr;
	Vector2i active_cell;
	Vector3 focus;
	Vector3 movement;
	Vector2i focus_cell = Vector2i(INT32_MAX, INT32_MAX);
	real_t view_distance = 256;
	int patch_size = 64;
	int epoch = 0;
	int64_t resident_triangles = 0;
	uint64_t next_attempt_usec = 0;
	bool enabled = true;
	bool paused = false;
	bool dirty = true;
	bool snapshot_ready = false;
	bool invalidate_resident = false;
	bool snapshot_bounded = false;
	AABB snapshot_area;
	TerrainData blueprint;
	int snapshot_workers = 1;
	bool snapshot_collision = true;
	bool snapshot_lod = true;
	real_t snapshot_lod_distance = 32;
	real_t snapshot_lod_error = 0.25;
	real_t snapshot_lod_near_error = 0;
	int snapshot_lod_max_level = 2;
	uint64_t started_usec = 0;
	String error;
	TerrainView *get_template() const;
	void plan();
	void tick();
	void retire_patch(TerrainView *p_view);
	real_t score(const Vector2i &p_cell) const;
	void patch_completed(const Dictionary &p_stats);
	void patch_failed(const String &p_error);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_template_path(const NodePath &p_value) { template_path = p_value; }
	NodePath get_template_path() const { return template_path; }
	void set_view_distance(real_t p_value);
	real_t get_view_distance() const { return view_distance; }
	void set_focus(const Vector3 &p_position, const Vector3 &p_direction = Vector3());
	void set_paused(bool p_value) { paused = p_value; }
	bool is_paused() const { return paused; }
	void set_enabled(bool p_enabled);
	bool is_enabled() const { return enabled; }
	void rebuild();
	Dictionary get_statistics() const;
	float sample_field(const Vector3 &p_position) const;
	bool has_terrain_at(const Vector3 &p_position) const;
};
