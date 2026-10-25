/**************************************************************************/
/*  world_region_layout.h                                                 */
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

#include "region_container.h"

// Parent-owned allocation. Child nodes are reusable weighted definitions, not
// positioned instances. The immutable modifier evaluates only demanded cells.
class WorldRegionLayout : public RegionContainer {
	GDCLASS(WorldRegionLayout, RegionContainer);

public:
	enum LayoutMode {
		GRID,
		VORONOI,
	};

private:
	LayoutMode layout_mode = GRID;
	real_t cell_jitter = 1.0;
	int cells_per_region = 4;
	Vector2 distribution_scale = Vector2(1, 1);
	real_t region_size = 24.0;
	real_t transition_width = 8.0;
	int seed = 2026;

protected:
	static void _bind_methods();

public:
	void set_layout_mode(LayoutMode p_mode);
	LayoutMode get_layout_mode() const;
	void set_cell_jitter(real_t p_jitter);
	real_t get_cell_jitter() const;
	void set_cells_per_region(int p_count);
	int get_cells_per_region() const;
	void set_distribution_scale(const Vector2 &p_scale);
	Vector2 get_distribution_scale() const;
	Dictionary get_region_at(const Vector3 &p_world_position) const;
	void set_region_size(real_t p_size);
	real_t get_region_size() const;
	void set_transition_width(real_t p_width);
	real_t get_transition_width() const;
	void set_seed(int p_seed);
	int get_seed() const;
	bool collect_operations(Vector<TerrainOperation> &r_operations, String &r_error, const Vector<AABB> &p_scopes = Vector<AABB>(), int p_depth = 0, bool p_assigned = false) const override;
};

VARIANT_ENUM_CAST(WorldRegionLayout::LayoutMode);
