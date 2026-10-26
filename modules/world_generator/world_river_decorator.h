/**************************************************************************/
/*  world_river_decorator.h                                             */
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
#include "world_decorator.h"

#include "scene/resources/mesh.h"

// Authored downhill centerline. Drainage-network discovery is a separate layer.
class WorldRiverDecorator : public WorldDecorator {
	GDCLASS(WorldRiverDecorator, WorldDecorator);
	PackedVector3Array path;
	real_t width = 6;
	real_t bank_width = 10;
	real_t depth = 3;
	real_t slope = 0;
	real_t max_cut_depth = 0;
	real_t meander = 4;
	int seed = 42;
	bool validate_path(String &r_error) const;
	PackedVector3Array build_path() const;

protected:
	static void _bind_methods();

public:
	WorldRiverDecorator();
	void set_centerline(const PackedVector3Array &p_value) {
		path = p_value;
		emit_changed();
	}
	PackedVector3Array get_centerline() const { return path; }
	void set_width(real_t p_value);
	real_t get_width() const { return width; }
	void set_bank_width(real_t p_value);
	real_t get_bank_width() const { return bank_width; }
	void set_max_cut_depth(real_t p_value);
	real_t get_max_cut_depth() const { return max_cut_depth; }
	void set_slope(real_t p_value);
	real_t get_slope() const { return slope; }
	void set_depth(real_t p_value);
	real_t get_depth() const { return depth; }
	void set_meander(real_t p_value);
	real_t get_meander() const { return meander; }
	void set_seed(int p_value) {
		seed = p_value;
		emit_changed();
	}
	int get_seed() const { return seed; }
	PackedVector3Array get_generated_centerline() const;
	Ref<ArrayMesh> create_water_mesh(const AABB &p_bounds = AABB()) const;
	Ref<TerrainModifier> compile(const Vector3 &p_origin, String &r_error) const override;
};
