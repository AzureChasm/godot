/**************************************************************************/
/*  world_region_decorators.cpp                                           */
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

#include "world_region_decorators.h"

#include "core/object/class_db.h"

namespace {
class RavineModifier : public TerrainModifier {
public:
	Vector3 center;
	real_t width, length, depth, meander, phase, probability, variation;
	uint32_t seed;
	real_t random(uint32_t p_instance, uint32_t p_salt) const {
		uint32_t value = p_instance ^ seed ^ p_salt;
		value ^= value >> 16;
		value *= 0x7feb352du;
		value ^= value >> 15;
		value *= 0x846ca68bu;
		value ^= value >> 16;
		return double(value) / 4294967296.0;
	}
	void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const override {
		apply_seeded(p_position, r_field, r_material, 0);
	}
	void apply_seeded(const Vector3 &p_position, float &r_field, uint16_t &r_material, uint32_t p_instance_seed) const override {
		if (probability <= 0 || (probability < 1 && random(p_instance_seed, 0x94d049bbu) >= probability)) {
			return;
		}
		const real_t shape = (random(p_instance_seed, 0x68bc21ebu) * 2 - 1) * variation;
		const real_t bend_phase = phase + shape * Math::PI;
		const real_t angle = shape * Math::PI;
		Vector3 local = p_position - center;
		if (variation > 0) {
			local.x -= shape * width;
			local.z -= (random(p_instance_seed, 0x9e3779b9u) * 2 - 1) * variation * length * 0.2;
			const real_t cosine = Math::cos(angle), sine = Math::sin(angle);
			local = Vector3(local.x * cosine - local.z * sine, local.y, local.x * sine + local.z * cosine);
		}
		const real_t varied_width = width * (1 + 0.35 * shape);
		const real_t varied_length = length * (1 - 0.25 * shape);
		const real_t varied_depth = depth * (1 + 0.3 * shape);
		const real_t bend = meander * (Math::sin(local.z * 6.0 / varied_length + bend_phase) + variation * 0.35 * Math::sin(local.z * 15.0 / varied_length - bend_phase));
		const real_t x = (local.x - bend) / (varied_width * 0.5);
		const real_t z = local.z / (varied_length * 0.5);
		const real_t horizontal = (Math::sqrt(x * x + z * z) - 1.0) * varied_width * 0.5;
		const real_t cut = MAX(horizontal, -varied_depth - local.y);
		r_field = MAX(r_field, float(-cut));
	}
};

class StrataModifier : public TerrainModifier {
public:
	real_t depth;
	uint16_t material;
	void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const override {
		if (r_field < -depth) {
			r_material = material;
		}
	}
};
} // namespace

void WorldBaseElevationDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_elevation", "value"), &WorldBaseElevationDecorator::set_elevation);
	ClassDB::bind_method(D_METHOD("get_elevation"), &WorldBaseElevationDecorator::get_elevation);
	ClassDB::bind_method(D_METHOD("set_relative", "value"), &WorldBaseElevationDecorator::set_relative);
	ClassDB::bind_method(D_METHOD("is_relative"), &WorldBaseElevationDecorator::is_relative);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "elevation", PROPERTY_HINT_RANGE, "-100,100,0.5,or_less,or_greater"), "set_elevation", "get_elevation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "relative"), "set_relative", "is_relative");
}

void WorldBaseElevationDecorator::set_elevation(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || Math::abs(p_value) > 100000);
	elevation = p_value;
	emit_changed();
}

Ref<TerrainModifier> WorldBaseElevationDecorator::compile(const Vector3 &p_origin, String &r_error) const {
	r_error = "Base elevation is metadata and must be resolved by RegionContainer.";
	return Ref<TerrainModifier>();
}

void WorldRavineDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_probability", "value"), &WorldRavineDecorator::set_probability);
	ClassDB::bind_method(D_METHOD("get_probability"), &WorldRavineDecorator::get_probability);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "probability", PROPERTY_HINT_RANGE, "0,1,0.05"), "set_probability", "get_probability");

	ClassDB::bind_method(D_METHOD("set_variation", "value"), &WorldRavineDecorator::set_variation);
	ClassDB::bind_method(D_METHOD("get_variation"), &WorldRavineDecorator::get_variation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "variation", PROPERTY_HINT_RANGE, "0,1,0.05"), "set_variation", "get_variation");

	ClassDB::bind_method(D_METHOD("set_center", "value"), &WorldRavineDecorator::set_center);
	ClassDB::bind_method(D_METHOD("get_center"), &WorldRavineDecorator::get_center);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center"), "set_center", "get_center");
	ClassDB::bind_method(D_METHOD("set_width", "value"), &WorldRavineDecorator::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &WorldRavineDecorator::get_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width", PROPERTY_HINT_RANGE, "0.1,64,0.1,or_greater"), "set_width", "get_width");
	ClassDB::bind_method(D_METHOD("set_length", "value"), &WorldRavineDecorator::set_length);
	ClassDB::bind_method(D_METHOD("get_length"), &WorldRavineDecorator::get_length);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "length", PROPERTY_HINT_RANGE, "0.1,128,0.1,or_greater"), "set_length", "get_length");
	ClassDB::bind_method(D_METHOD("set_depth", "value"), &WorldRavineDecorator::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &WorldRavineDecorator::get_depth);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth", PROPERTY_HINT_RANGE, "0.1,128,0.1,or_greater"), "set_depth", "get_depth");
	ClassDB::bind_method(D_METHOD("set_meander", "value"), &WorldRavineDecorator::set_meander);
	ClassDB::bind_method(D_METHOD("get_meander"), &WorldRavineDecorator::get_meander);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "meander", PROPERTY_HINT_RANGE, "0,32,0.1,or_greater"), "set_meander", "get_meander");
	ClassDB::bind_method(D_METHOD("set_seed", "value"), &WorldRavineDecorator::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &WorldRavineDecorator::get_seed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
}

void WorldRavineDecorator::set_probability(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	probability = p_value;
	emit_changed();
}

void WorldRavineDecorator::set_variation(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	variation = p_value;
	emit_changed();
}

void WorldRavineDecorator::set_width(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 100000);
	width = p_value;
	emit_changed();
}

void WorldRavineDecorator::set_length(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 100000);
	length = p_value;
	emit_changed();
}

void WorldRavineDecorator::set_depth(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value <= 0 || p_value > 100000);
	depth = p_value;
	emit_changed();
}

void WorldRavineDecorator::set_meander(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 100000);
	meander = p_value;
	emit_changed();
}

void WorldRavineDecorator::set_center(const Vector3 &p_value) {
	ERR_FAIL_COND(!p_value.is_finite());
	center = p_value;
	emit_changed();
}

Ref<TerrainModifier> WorldRavineDecorator::compile(const Vector3 &p_origin, String &r_error) const {
	Ref<RavineModifier> result;
	result.instantiate();
	result->center = p_origin + center;
	result->width = width;
	result->length = length;
	result->depth = depth;
	result->meander = meander;
	result->probability = probability;
	result->variation = variation;
	result->seed = uint32_t(seed);
	result->phase = (uint32_t(seed) % 65536) * (Math::TAU / 65536.0);
	return result;
}

void WorldStrataDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_soil_depth", "value"), &WorldStrataDecorator::set_soil_depth);
	ClassDB::bind_method(D_METHOD("get_soil_depth"), &WorldStrataDecorator::get_soil_depth);
	ClassDB::bind_method(D_METHOD("set_rock_material", "value"), &WorldStrataDecorator::set_rock_material);
	ClassDB::bind_method(D_METHOD("get_rock_material"), &WorldStrataDecorator::get_rock_material);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "soil_depth", PROPERTY_HINT_RANGE, "0,16,0.1,or_greater"), "set_soil_depth", "get_soil_depth");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rock_material", PROPERTY_HINT_RANGE, "1,65535,1"), "set_rock_material", "get_rock_material");
}

void WorldStrataDecorator::set_soil_depth(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 100000);
	soil_depth = p_value;
	emit_changed();
}

void WorldStrataDecorator::set_rock_material(int p_value) {
	ERR_FAIL_COND(p_value < 1 || p_value > 65535);
	rock_material = p_value;
	emit_changed();
}

Ref<TerrainModifier> WorldStrataDecorator::compile(const Vector3 &p_origin, String &r_error) const {
	Ref<StrataModifier> result;
	result.instantiate();
	result->depth = soil_depth;
	result->material = rock_material;
	return result;
}
