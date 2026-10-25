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
	};

protected:
	enum Profile {
		PLAINS,
		HILLS,
		MOUNTAINS,
		SEA,
		DEEP_SEA,
	};
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

public:
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

VARIANT_ENUM_CAST(WorldTerrainDecorator::BlendMode);
