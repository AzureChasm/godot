/**************************************************************************/
/*  world_decorator.h                                                     */
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

#include "core/io/resource.h"

class WorldDecorator : public Resource {
	GDCLASS(WorldDecorator, Resource);

	bool enabled = true;
	int priority = 0;

protected:
	static void _bind_methods();

public:
	void set_enabled(bool p_enabled);
	bool is_enabled() const;
	void set_priority(int p_priority);
	int get_priority() const;

	// Spatial primitives keep explicit coordinates unless they opt into the regional datum.
	virtual bool uses_base_elevation() const { return false; }
	virtual Ref<TerrainModifier> compile(const Vector3 &p_origin, String &r_error) const = 0;
};

class WorldSphereDecorator : public WorldDecorator {
	GDCLASS(WorldSphereDecorator, WorldDecorator);

public:
	enum Operation {
		ADD_SOLID,
		REMOVE_SOLID,
	};

private:
	Vector3 center;
	real_t radius = 1.0;
	Operation operation = ADD_SOLID;
	int material_id = 1;

protected:
	static void _bind_methods();

public:
	void set_center(const Vector3 &p_center);
	Vector3 get_center() const;
	void set_radius(real_t p_radius);
	real_t get_radius() const;
	void set_operation(Operation p_operation);
	Operation get_operation() const;
	void set_material_id(int p_material_id);
	int get_material_id() const;
	Ref<TerrainModifier> compile(const Vector3 &p_origin, String &r_error) const override;
};

VARIANT_ENUM_CAST(WorldSphereDecorator::Operation);
