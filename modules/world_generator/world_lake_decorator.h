/**************************************************************************/
/*  world_lake_decorator.h                                              */
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

// A grounded basin with a closed shore and an independent horizontal water level.
class WorldLakeDecorator : public WorldDecorator {
	GDCLASS(WorldLakeDecorator, WorldDecorator);
	Vector3 center;
	Vector2 radius = Vector2(40, 28);
	real_t depth = 8;
	real_t bank_width = 24;
	real_t shore_height = 2;
	real_t shore_noise = 0.15;
	int seed = 42;

protected:
	static void _bind_methods();

public:
	WorldLakeDecorator() { set_priority(-40); }
	void set_center(const Vector3 &p_value);
	Vector3 get_center() const { return center; }
	void set_radius(const Vector2 &p_value);
	Vector2 get_radius() const { return radius; }
	void set_depth(real_t p_value);
	real_t get_depth() const { return depth; }
	void set_bank_width(real_t p_value);
	real_t get_bank_width() const { return bank_width; }
	void set_shore_height(real_t p_value);
	real_t get_shore_height() const { return shore_height; }
	void set_shore_noise(real_t p_value);
	real_t get_shore_noise() const { return shore_noise; }
	void set_seed(int p_value) {
		seed = p_value;
		emit_changed();
	}
	int get_seed() const { return seed; }
	Ref<ArrayMesh> conform_water_mesh(const Ref<ArrayMesh> &p_mesh) const;
	Ref<ArrayMesh> create_water_mesh(const AABB &p_bounds = AABB()) const;
	Ref<TerrainModifier> compile(const Vector3 &p_origin, String &r_error) const override;
};
