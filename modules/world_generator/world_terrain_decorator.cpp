/**************************************************************************/
/*  world_terrain_decorator.cpp                                           */
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

#include "world_terrain_decorator.h"

#include "core/object/class_db.h"

#include <thirdparty/misc/FastNoiseLite.h>

namespace {
class SurfaceProfileModifier : public TerrainModifier {
public:
	Vector3 origin;
	real_t elevation = 0;
	real_t amplitude = 1;
	real_t wavelength = 5;
	int octaves = 3;
	bool ridged = false;
	bool deep_sea = false;
	uint16_t material = 2;
	WorldTerrainDecorator::BlendMode blend_mode = WorldTerrainDecorator::REPLACE_TERRAIN;
	fastnoiselite::FastNoiseLite noise;

	bool is_column_profile() const override { return true; }
	void apply(const Vector3 &p_position, float &r_field, uint16_t &r_material) const override {
		const Vector3 local = p_position - origin;
		real_t frequency = 1.0 / wavelength;
		real_t weight = 1;
		real_t total_weight = 0;
		real_t relief = 0;
		for (int octave = 0; octave < octaves; ++octave) {
			real_t value = noise.GetNoise(float(local.x * frequency), float(local.z * frequency));
			if (ridged) {
				// Sharpen the crest of each octave instead of merely increasing noise amplitude.
				const real_t ridge = 1.0 - Math::abs(value);
				value = ridge * ridge;
			} else if (deep_sea) {
				value = 0.7 * value - 0.3 * Math::abs(value);
			}
			relief += value * weight;
			total_weight += weight;
			frequency *= 2;
			// Keep mountain massifs broad; high-frequency octaves should add detail,
			// not turn an entire mountain into a field of needle-like peaks.
			weight *= ridged ? 0.3 : 0.5;
		}
		const float field = local.y - elevation - amplitude * relief / total_weight;
		switch (blend_mode) {
			case WorldTerrainDecorator::REPLACE_TERRAIN:
				r_field = field;
				r_material = material;
				break;
			case WorldTerrainDecorator::ADD_SOLID:
				if (field < r_field) {
					r_field = field;
					r_material = material;
				}
				break;
			case WorldTerrainDecorator::REMOVE_SOLID:
				r_field = MAX(r_field, field);
				break;
		}
	}
};
} // namespace

WorldTerrainDecorator::WorldTerrainDecorator(real_t p_elevation, real_t p_amplitude, real_t p_wavelength, int p_octaves, int p_material) {
	elevation = p_elevation;
	amplitude = p_amplitude;
	wavelength = p_wavelength;
	octaves = p_octaves;
	material_id = p_material;
	set_priority(-100);
}

void WorldTerrainDecorator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_elevation", "elevation"), &WorldTerrainDecorator::set_elevation);
	ClassDB::bind_method(D_METHOD("get_elevation"), &WorldTerrainDecorator::get_elevation);
	ClassDB::bind_method(D_METHOD("set_amplitude", "amplitude"), &WorldTerrainDecorator::set_amplitude);
	ClassDB::bind_method(D_METHOD("get_amplitude"), &WorldTerrainDecorator::get_amplitude);
	ClassDB::bind_method(D_METHOD("set_wavelength", "wavelength"), &WorldTerrainDecorator::set_wavelength);
	ClassDB::bind_method(D_METHOD("get_wavelength"), &WorldTerrainDecorator::get_wavelength);
	ClassDB::bind_method(D_METHOD("set_octaves", "octaves"), &WorldTerrainDecorator::set_octaves);
	ClassDB::bind_method(D_METHOD("get_octaves"), &WorldTerrainDecorator::get_octaves);
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &WorldTerrainDecorator::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &WorldTerrainDecorator::get_seed);
	ClassDB::bind_method(D_METHOD("set_material_id", "material_id"), &WorldTerrainDecorator::set_material_id);
	ClassDB::bind_method(D_METHOD("get_material_id"), &WorldTerrainDecorator::get_material_id);
	ClassDB::bind_method(D_METHOD("set_blend_mode", "blend_mode"), &WorldTerrainDecorator::set_blend_mode);
	ClassDB::bind_method(D_METHOD("get_blend_mode"), &WorldTerrainDecorator::get_blend_mode);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "elevation", PROPERTY_HINT_RANGE, "-20,20,0.05,or_less,or_greater"), "set_elevation", "get_elevation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "amplitude", PROPERTY_HINT_RANGE, "0,10,0.05,or_greater"), "set_amplitude", "get_amplitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wavelength", PROPERTY_HINT_RANGE, "0.25,100,0.05,or_greater"), "set_wavelength", "get_wavelength");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "octaves", PROPERTY_HINT_RANGE, "1,6,1"), "set_octaves", "get_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_id", PROPERTY_HINT_RANGE, "1,65535,1"), "set_material_id", "get_material_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_mode", PROPERTY_HINT_ENUM, "Replace Terrain,Add Solid,Remove Solid"), "set_blend_mode", "get_blend_mode");
	BIND_ENUM_CONSTANT(REPLACE_TERRAIN);
	BIND_ENUM_CONSTANT(ADD_SOLID);
	BIND_ENUM_CONSTANT(REMOVE_SOLID);
}

void WorldTerrainDecorator::set_elevation(real_t p_elevation) {
	ERR_FAIL_COND(!Math::is_finite(p_elevation) || Math::abs(p_elevation) > 100000);
	elevation = p_elevation;
	emit_changed();
}
real_t WorldTerrainDecorator::get_elevation() const {
	return elevation;
}

void WorldTerrainDecorator::set_amplitude(real_t p_amplitude) {
	ERR_FAIL_COND(!Math::is_finite(p_amplitude) || p_amplitude < 0 || p_amplitude > 100000);
	amplitude = p_amplitude;
	emit_changed();
}
real_t WorldTerrainDecorator::get_amplitude() const {
	return amplitude;
}

void WorldTerrainDecorator::set_wavelength(real_t p_wavelength) {
	ERR_FAIL_COND(!Math::is_finite(p_wavelength) || p_wavelength < 0.25 || p_wavelength > 100000);
	wavelength = p_wavelength;
	emit_changed();
}
real_t WorldTerrainDecorator::get_wavelength() const {
	return wavelength;
}

void WorldTerrainDecorator::set_octaves(int p_octaves) {
	ERR_FAIL_COND(p_octaves < 1 || p_octaves > 6);
	octaves = p_octaves;
	emit_changed();
}
int WorldTerrainDecorator::get_octaves() const {
	return octaves;
}

void WorldTerrainDecorator::set_seed(int p_seed) {
	seed = p_seed;
	emit_changed();
}
int WorldTerrainDecorator::get_seed() const {
	return seed;
}

void WorldTerrainDecorator::set_material_id(int p_material_id) {
	ERR_FAIL_COND(p_material_id < 1 || p_material_id > 65535);
	material_id = p_material_id;
	emit_changed();
}
int WorldTerrainDecorator::get_material_id() const {
	return material_id;
}

void WorldTerrainDecorator::set_blend_mode(BlendMode p_mode) {
	ERR_FAIL_COND(p_mode < REPLACE_TERRAIN || p_mode > REMOVE_SOLID);
	blend_mode = p_mode;
	emit_changed();
}
WorldTerrainDecorator::BlendMode WorldTerrainDecorator::get_blend_mode() const {
	return blend_mode;
}

Ref<TerrainModifier> WorldTerrainDecorator::compile(const Vector3 &p_origin, String &r_error) const {
	if (!p_origin.is_finite() || p_origin.length() > 100000) {
		r_error = "Terrain-profile container origin exceeds the prototype coordinate budget.";
		return Ref<TerrainModifier>();
	}
	Ref<SurfaceProfileModifier> result;
	result.instantiate();
	result->origin = p_origin;
	result->elevation = elevation;
	result->amplitude = amplitude;
	result->wavelength = wavelength;
	result->octaves = octaves;
	result->material = material_id;
	result->blend_mode = blend_mode;
	result->ridged = get_profile() == MOUNTAINS;
	result->deep_sea = get_profile() == DEEP_SEA;
	result->noise.SetSeed(seed);
	result->noise.SetFrequency(1.0);
	result->noise.SetNoiseType(fastnoiselite::FastNoiseLite::NoiseType_OpenSimplex2S);
	return result;
}
