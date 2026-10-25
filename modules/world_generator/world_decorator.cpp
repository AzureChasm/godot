/**************************************************************************/
/*  world_decorator.cpp                                                   */
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

#include "world_decorator.h"

#include "core/object/class_db.h"

namespace {
class SphereModifier : public TerrainModifier {
public:
	Vector3 center;
	real_t radius = 1;
	bool remove = false;
	uint16_t material = 1;

	void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const override {
		const float distance = p_position.distance_to(center) - radius;
		if (remove) {
			r_field = MAX(r_field, -distance);
		} else if (distance < r_field) {
			r_field = distance;
			r_material = material;
		}
	}
};
} // namespace

void WorldDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &WorldDecorator::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &WorldDecorator::is_enabled);
	ClassDB::bind_method(D_METHOD("set_priority", "priority"), &WorldDecorator::set_priority);
	ClassDB::bind_method(D_METHOD("get_priority"), &WorldDecorator::get_priority);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "priority"), "set_priority", "get_priority");
}

void WorldDecorator::set_enabled(bool p_enabled) {
	enabled = p_enabled;
	emit_changed();
}

bool WorldDecorator::is_enabled() const {
	return enabled;
}

void WorldDecorator::set_priority(int p_priority) {
	priority = p_priority;
	emit_changed();
}

int WorldDecorator::get_priority() const {
	return priority;
}

void WorldSphereDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_center", "center"), &WorldSphereDecorator::set_center);
	ClassDB::bind_method(D_METHOD("get_center"), &WorldSphereDecorator::get_center);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &WorldSphereDecorator::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &WorldSphereDecorator::get_radius);
	ClassDB::bind_method(D_METHOD("set_operation", "operation"), &WorldSphereDecorator::set_operation);
	ClassDB::bind_method(D_METHOD("get_operation"), &WorldSphereDecorator::get_operation);
	ClassDB::bind_method(D_METHOD("set_material_id", "material_id"), &WorldSphereDecorator::set_material_id);
	ClassDB::bind_method(D_METHOD("get_material_id"), &WorldSphereDecorator::get_material_id);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center"), "set_center", "get_center");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.01,100,0.01,or_greater"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "operation", PROPERTY_HINT_ENUM, "Add Solid,Remove Solid"), "set_operation", "get_operation");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_id", PROPERTY_HINT_RANGE, "1,65535,1"), "set_material_id", "get_material_id");
	BIND_ENUM_CONSTANT(ADD_SOLID);
	BIND_ENUM_CONSTANT(REMOVE_SOLID);
}

void WorldSphereDecorator::set_center(const Vector3 &p_center) {
	ERR_FAIL_COND(!p_center.is_finite());
	center = p_center;
	emit_changed();
}

Vector3 WorldSphereDecorator::get_center() const {
	return center;
}

void WorldSphereDecorator::set_radius(real_t p_radius) {
	ERR_FAIL_COND(!Math::is_finite(p_radius) || p_radius <= 0);
	radius = p_radius;
	emit_changed();
}

real_t WorldSphereDecorator::get_radius() const {
	return radius;
}

void WorldSphereDecorator::set_operation(Operation p_operation) {
	ERR_FAIL_COND(p_operation != ADD_SOLID && p_operation != REMOVE_SOLID);
	operation = p_operation;
	emit_changed();
}

WorldSphereDecorator::Operation WorldSphereDecorator::get_operation() const {
	return operation;
}

void WorldSphereDecorator::set_material_id(int p_material_id) {
	ERR_FAIL_COND(p_material_id < 1 || p_material_id > 65535);
	material_id = p_material_id;
	emit_changed();
}

int WorldSphereDecorator::get_material_id() const {
	return material_id;
}

Ref<TerrainModifier> WorldSphereDecorator::compile(const Vector3 &p_origin, String &r_error) const {
	if (!center.is_finite() || !Math::is_finite(radius) || radius <= 0 || !(center + p_origin).is_finite()) {
		r_error = "Sphere center/radius is invalid.";
		return Ref<TerrainModifier>();
	}
	Ref<SphereModifier> result;
	result.instantiate();
	result->center = center + p_origin;
	result->radius = radius;
	result->remove = operation == REMOVE_SOLID;
	result->material = material_id;
	return result;
}
