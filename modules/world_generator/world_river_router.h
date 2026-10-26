/**************************************************************************/
/*  world_river_router.h                                                  */
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

#include "core/os/thread.h"

// Explicit, world-anchored drainage study, independent of camera residency.
// Captures terrain on the main thread and plans only on CPU workers.
class WorldRiverRouter : public RefCounted {
	GDCLASS(WorldRiverRouter, RefCounted);
	struct Job;
	Job *job = nullptr;
	String last_error;
	real_t minimum_source_height = 64;
	real_t max_incision = 6;
	real_t source_separation = 192;
	real_t edge_noise = 0.18;
	real_t minimum_grade = 0.0005;

protected:
	static void _bind_methods();

public:
	~WorldRiverRouter();
	bool begin(RegionContainer *p_root, const AABB &p_area, int p_seed = 2026, real_t p_spacing = 48, int p_max_rivers = 6);
	void cancel();
	bool is_running() const;
	Dictionary get_statistics() const;
	Array get_paths() const;
	Dictionary get_lakes() const;
	void set_minimum_source_height(real_t p_value);
	real_t get_minimum_source_height() const { return minimum_source_height; }
	void set_max_incision(real_t p_value);
	real_t get_max_incision() const { return max_incision; }
	void set_source_separation(real_t p_value);
	real_t get_source_separation() const { return source_separation; }
	void set_minimum_grade(real_t p_value);
	real_t get_minimum_grade() const { return minimum_grade; }
	void set_edge_noise(real_t p_value);
	real_t get_edge_noise() const { return edge_noise; }
};
