/**************************************************************************/
/*  world_region_decorators.h                                             */
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

// Authoring metadata, resolved before compiling descendant terrain operations.
class WorldBaseElevationDecorator : public WorldDecorator {
	GDCLASS(WorldBaseElevationDecorator, WorldDecorator);
	real_t elevation = 0;
	bool relative = false;

protected:
	static void _bind_methods();

public:
	void set_elevation(real_t p_value);
	real_t get_elevation() const { return elevation; }
	void set_relative(bool p_value) {
		relative = p_value;
		emit_changed();
	}
	bool is_relative() const { return relative; }
	Ref<TerrainModifier> compile(const Vector3 &p_origin, String &r_error) const override;
};

class WorldRavineDecorator : public WorldDecorator {
	GDCLASS(WorldRavineDecorator, WorldDecorator);
	Vector3 center;
	real_t width = 5;
	real_t length = 24;
	real_t depth = 12;
	real_t meander = 1;
	int seed = 42;
	real_t probability = 1;
	real_t variation = 0;

protected:
	static void _bind_methods();

public:
	void set_probability(real_t p_value);
	real_t get_probability() const { return probability; }
	void set_variation(real_t p_value);
	real_t get_variation() const { return variation; }
	void set_center(const Vector3 &p_value);
	Vector3 get_center() const { return center; }
	void set_width(real_t p_value);
	real_t get_width() const { return width; }
	void set_length(real_t p_value);
	real_t get_length() const { return length; }
	void set_depth(real_t p_value);
	real_t get_depth() const { return depth; }
	void set_meander(real_t p_value);
	real_t get_meander() const { return meander; }
	void set_seed(int p_value) {
		seed = p_value;
		emit_changed();
	}
	int get_seed() const { return seed; }
	bool uses_base_elevation() const override { return true; }
	Ref<TerrainModifier> compile(const Vector3 &p_origin, String &r_error) const override;
};

// Paint the existing solid by depth in its incoming terrain field; never fills cuts.
class WorldStrataDecorator : public WorldDecorator {
	GDCLASS(WorldStrataDecorator, WorldDecorator);
	real_t soil_depth = 2;
	int rock_material = 3;

protected:
	static void _bind_methods();

public:
	WorldStrataDecorator() { set_priority(-50); }
	void set_soil_depth(real_t p_value);
	real_t get_soil_depth() const { return soil_depth; }
	void set_rock_material(int p_value);
	int get_rock_material() const { return rock_material; }
	Ref<TerrainModifier> compile(const Vector3 &p_origin, String &r_error) const override;
};
