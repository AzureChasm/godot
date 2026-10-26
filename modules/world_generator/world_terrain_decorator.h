/**************************************************************************/
/*  world_terrain_decorator.h                                             */
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

// Shared tunable surface-profile contract. The result is still a 3D signed
// field, so later sphere/cave decorators can carve it or add floating solids.
class WorldTerrainDecorator : public WorldDecorator {
	GDCLASS(WorldTerrainDecorator, WorldDecorator);

public:
	enum BlendMode {
		REPLACE_TERRAIN,
		ADD_SOLID,
		REMOVE_SOLID,
		ADD_RELIEF,
	};

	enum Profile {
		PLAINS,
		HILLS,
		MOUNTAINS,
		SEA,
		DEEP_SEA,
		MOUNTAIN_RANGE,
		FOOTHILLS,
		PLATEAU,
		MESA,
		DUNES,
		BASIN,
		TERRACES,
		VALLEY,
		ESCARPMENT,
	};

protected:
	static void _bind_methods();
	virtual Profile get_profile() const = 0;
	WorldTerrainDecorator(real_t p_elevation, real_t p_amplitude, real_t p_wavelength, int p_octaves, int p_material);

private:
	real_t elevation = 0;
	real_t amplitude = 1;
	real_t wavelength = 5;
	int octaves = 3;
	int seed = 13579;
	int material_id = 2;
	BlendMode blend_mode = REPLACE_TERRAIN;
	bool world_space_noise = false;
	real_t interior_elevation = 0;
	real_t interior_scale = 1;
	real_t interior_width = 12;
	real_t roughness = -1;
	real_t ridge_sharpness = 2;
	real_t detail_strength = 0.03;
	real_t ridge_width = 0.18;
	int summit_count = 3;
	int terrace_steps = 6;
	real_t edge_strength = 1;
	real_t summit_variation = 0.28;
	real_t summit_rounding = 0;
	real_t summit_breadth = 0.46;
	real_t range_irregularity = 0;
	real_t summit_height_bias = 1;
	bool hierarchical_summits = false;

public:
	void set_world_space_noise(bool p_value);
	bool get_world_space_noise() const { return world_space_noise; }
	void set_interior_elevation(real_t p_value);
	real_t get_interior_elevation() const { return interior_elevation; }
	void set_interior_scale(real_t p_value);
	real_t get_interior_scale() const { return interior_scale; }
	void set_interior_width(real_t p_value);
	real_t get_interior_width() const { return interior_width; }
	void set_roughness(real_t p_value);
	real_t get_roughness() const { return roughness; }
	void set_ridge_sharpness(real_t p_value);
	real_t get_ridge_sharpness() const { return ridge_sharpness; }
	void set_detail_strength(real_t p_value);
	real_t get_detail_strength() const { return detail_strength; }
	void set_ridge_width(real_t p_value);
	real_t get_ridge_width() const { return ridge_width; }
	void set_summit_count(int p_value);
	int get_summit_count() const { return summit_count; }
	void set_terrace_steps(int p_value);
	int get_terrace_steps() const { return terrace_steps; }
	void set_edge_strength(real_t p_value);
	real_t get_edge_strength() const { return edge_strength; }
	void set_summit_variation(real_t p_value);
	real_t get_summit_variation() const { return summit_variation; }
	void set_summit_rounding(real_t p_value);
	real_t get_summit_rounding() const { return summit_rounding; }
	void set_summit_breadth(real_t p_value);
	real_t get_summit_breadth() const { return summit_breadth; }
	void set_range_irregularity(real_t p_value);
	real_t get_range_irregularity() const { return range_irregularity; }
	void set_summit_height_bias(real_t p_value);
	real_t get_summit_height_bias() const { return summit_height_bias; }
	void set_hierarchical_summits(bool p_enabled);
	bool get_hierarchical_summits() const { return hierarchical_summits; }
	void set_elevation(real_t p_elevation);
	real_t get_elevation() const;
	void set_amplitude(real_t p_amplitude);
	real_t get_amplitude() const;
	void set_wavelength(real_t p_wavelength);
	real_t get_wavelength() const;
	void set_octaves(int p_octaves);
	int get_octaves() const;
	void set_seed(int p_seed);
	int get_seed() const;
	void set_material_id(int p_material_id);
	int get_material_id() const;
	void set_blend_mode(BlendMode p_mode);
	BlendMode get_blend_mode() const;
	bool uses_base_elevation() const override { return true; }
	Ref<TerrainModifier> compile(const Vector3 &p_origin, String &r_error) const override;
};

class WorldPlainsDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldPlainsDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return PLAINS; }

public:
	WorldPlainsDecorator() : WorldTerrainDecorator(0.1, 0.08, 5.0, 2, 2) {}
};

class WorldHillsDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldHillsDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return HILLS; }

public:
	WorldHillsDecorator() : WorldTerrainDecorator(0.65, 1.1, 4.5, 3, 2) {}
};

class WorldMountainsDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldMountainsDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return MOUNTAINS; }

public:
	WorldMountainsDecorator() : WorldTerrainDecorator(0.3, 3.3, 8.0, 3, 3) {}
};

class WorldSeaDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldSeaDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return SEA; }

public:
	WorldSeaDecorator() : WorldTerrainDecorator(-1.5, 0.25, 5.0, 3, 4) {}
};

class WorldDeepSeaDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldDeepSeaDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return DEEP_SEA; }

public:
	WorldDeepSeaDecorator() : WorldTerrainDecorator(-3.5, 0.7, 3.5, 4, 5) {}
};

class WorldMountainRangeDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldMountainRangeDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return MOUNTAIN_RANGE; }

public:
	WorldMountainRangeDecorator() : WorldTerrainDecorator(0.0, 28.0, 96.0, 3, 3) {}
};

class WorldFoothillsDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldFoothillsDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return FOOTHILLS; }

public:
	WorldFoothillsDecorator() : WorldTerrainDecorator(0.0, 4.0, 40.0, 3, 2) {}
};

class WorldPlateauDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldPlateauDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return PLATEAU; }

public:
	WorldPlateauDecorator() : WorldTerrainDecorator(0.0, 7.0, 48.0, 3, 3) {}
};

class WorldMesaDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldMesaDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return MESA; }

public:
	WorldMesaDecorator() : WorldTerrainDecorator(0.0, 12.0, 48.0, 3, 3) {}
};

class WorldDunesDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldDunesDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return DUNES; }

public:
	WorldDunesDecorator() : WorldTerrainDecorator(0.0, 2.0, 18.0, 3, 4) {}
};

class WorldBasinDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldBasinDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return BASIN; }

public:
	WorldBasinDecorator() : WorldTerrainDecorator(0.0, 5.0, 48.0, 3, 2) {}
};

class WorldTerracesDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldTerracesDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return TERRACES; }

public:
	WorldTerracesDecorator() : WorldTerrainDecorator(0.0, 6.0, 40.0, 3, 3) {}
};

class WorldValleyDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldValleyDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return VALLEY; }

public:
	WorldValleyDecorator() : WorldTerrainDecorator(0.0, 4.0, 48.0, 3, 2) {}
};

class WorldEscarpmentDecorator : public WorldTerrainDecorator {
	GDCLASS(WorldEscarpmentDecorator, WorldTerrainDecorator);

protected:
	static void _bind_methods() {}
	Profile get_profile() const override { return ESCARPMENT; }

public:
	WorldEscarpmentDecorator() : WorldTerrainDecorator(0.0, 8.0, 64.0, 3, 3) {}
};

VARIANT_ENUM_CAST(WorldTerrainDecorator::BlendMode);
