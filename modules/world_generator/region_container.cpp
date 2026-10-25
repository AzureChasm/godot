/**************************************************************************/
/*  region_container.cpp                                                  */
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

#include "region_container.h"

#include "core/object/class_db.h"

void RegionContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_occurrence_weight", "weight"), &RegionContainer::set_occurrence_weight);
	ClassDB::bind_method(D_METHOD("get_occurrence_weight"), &RegionContainer::get_occurrence_weight);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "occurrence_weight", PROPERTY_HINT_RANGE, "0,100,0.1,or_greater"), "set_occurrence_weight", "get_occurrence_weight");
	ClassDB::bind_method(D_METHOD("set_bounded", "bounded"), &RegionContainer::set_bounded);
	ClassDB::bind_method(D_METHOD("is_bounded"), &RegionContainer::is_bounded);
	ClassDB::bind_method(D_METHOD("set_area", "area"), &RegionContainer::set_area);
	ClassDB::bind_method(D_METHOD("get_area"), &RegionContainer::get_area);
	ClassDB::bind_method(D_METHOD("set_decorators", "decorators"), &RegionContainer::set_decorators);
	ClassDB::bind_method(D_METHOD("get_decorators"), &RegionContainer::get_decorators);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bounded"), "set_bounded", "is_bounded");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "area"), "set_area", "get_area");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "decorators", PROPERTY_HINT_ARRAY_TYPE, "WorldDecorator"), "set_decorators", "get_decorators");
}

void RegionContainer::set_bounded(bool p_bounded) {
	bounded = p_bounded;
}

bool RegionContainer::is_bounded() const {
	return bounded;
}

void RegionContainer::set_area(const AABB &p_area) {
	area = p_area;
}

AABB RegionContainer::get_area() const {
	return area;
}

void RegionContainer::set_decorators(const TypedArray<WorldDecorator> &p_decorators) {
	decorators = p_decorators;
}

TypedArray<WorldDecorator> RegionContainer::get_decorators() const {
	return decorators;
}

void RegionContainer::set_occurrence_weight(real_t p_weight) {
	ERR_FAIL_COND(!Math::is_finite(p_weight) || p_weight < 0 || p_weight > 1000000);
	occurrence_weight = p_weight;
}
real_t RegionContainer::get_occurrence_weight() const {
	return occurrence_weight;
}

bool RegionContainer::collect_local_operations(Vector<TerrainOperation> &r_operations, String &r_error, Vector<AABB> &scopes, int p_depth, bool p_assigned) const {
	if (p_depth > 64 || r_operations.size() > 128) {
		r_error = "Prototype budget exceeded: at most 64 nesting levels and 128 decorators.";
		return false;
	}
	const Transform3D transform = get_global_transform();
	if (!transform.origin.is_finite() || !transform.basis.is_equal_approx(Basis())) {
		r_error = String(get_path()) + ": the first prototype supports translated, axis-aligned containers only (no rotation/scale).";
		return false;
	}
	if (bounded && !p_assigned) {
		if (!area.position.is_finite() || !area.size.is_finite() || area.size.x <= 0 || area.size.y <= 0 || area.size.z <= 0) {
			r_error = String(get_path()) + ": bounded area must be finite with positive dimensions.";
			return false;
		}
		scopes.push_back(AABB(area.position + transform.origin, area.size));
	}
	for (int i = 0; i < decorators.size(); ++i) {
		Ref<WorldDecorator> decorator = decorators[i];
		if (decorator.is_null() || !decorator->is_enabled()) {
			continue;
		}
		TerrainOperation operation;
		operation.modifier = decorator->compile(transform.origin, r_error);
		if (operation.modifier.is_null()) {
			return false;
		}
		operation.scopes = scopes;
		operation.priority = decorator->get_priority();
		operation.sequence = r_operations.size();
		r_operations.push_back(operation);
		if (r_operations.size() > 128) {
			r_error = "Prototype budget exceeded: at most 128 decorators.";
			return false;
		}
	}
	return true;
}

bool RegionContainer::collect_operations(Vector<TerrainOperation> &r_operations, String &r_error, const Vector<AABB> &p_scopes, int p_depth, bool p_assigned) const {
	Vector<AABB> scopes = p_scopes;
	if (!collect_local_operations(r_operations, r_error, scopes, p_depth, p_assigned)) {
		return false;
	}
	for (int i = 0; i < get_child_count(); ++i) {
		const RegionContainer *child = Object::cast_to<RegionContainer>(get_child(i));
		if (!child) {
			r_error = String(get_path()) + ": generation hierarchy children must be RegionContainer nodes.";
			return false;
		}
		if (!child->collect_operations(r_operations, r_error, scopes, p_depth + 1)) {
			return false;
		}
	}
	return true;
}
