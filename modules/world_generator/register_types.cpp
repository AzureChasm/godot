/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "region_container.h"
#include "terrain_streamer.h"
#include "terrain_view.h"
#include "world_decorator.h"
#include "world_lake_decorator.h"
#include "world_region_decorators.h"
#include "world_region_layout.h"
#include "world_river_decorator.h"
#include "world_river_router.h"
#include "world_terrain_decorator.h"

#include "core/object/class_db.h"

void initialize_world_generator_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_ABSTRACT_CLASS(WorldDecorator);
	GDREGISTER_CLASS(WorldSphereDecorator);
	GDREGISTER_CLASS(WorldBaseElevationDecorator);
	GDREGISTER_CLASS(WorldRavineDecorator);
	GDREGISTER_CLASS(WorldRiverDecorator);
	GDREGISTER_CLASS(WorldRiverRouter);
	GDREGISTER_CLASS(WorldLakeDecorator);
	GDREGISTER_CLASS(WorldStrataDecorator);
	GDREGISTER_ABSTRACT_CLASS(WorldTerrainDecorator);
	GDREGISTER_CLASS(WorldPlainsDecorator);
	GDREGISTER_CLASS(WorldHillsDecorator);
	GDREGISTER_CLASS(WorldMountainsDecorator);
	GDREGISTER_CLASS(WorldSeaDecorator);
	GDREGISTER_CLASS(WorldDeepSeaDecorator);
	GDREGISTER_CLASS(WorldMountainRangeDecorator);
	GDREGISTER_CLASS(WorldFoothillsDecorator);
	GDREGISTER_CLASS(WorldPlateauDecorator);
	GDREGISTER_CLASS(WorldMesaDecorator);
	GDREGISTER_CLASS(WorldDunesDecorator);
	GDREGISTER_CLASS(WorldBasinDecorator);
	GDREGISTER_CLASS(WorldTerracesDecorator);
	GDREGISTER_CLASS(WorldValleyDecorator);
	GDREGISTER_CLASS(WorldEscarpmentDecorator);
	GDREGISTER_CLASS(RegionContainer);
	GDREGISTER_CLASS(WorldRegionLayout);
	GDREGISTER_CLASS(TerrainView);
	GDREGISTER_CLASS(TerrainStreamer);
}

void uninitialize_world_generator_module(ModuleInitializationLevel p_level) {
}
