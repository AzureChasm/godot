/**************************************************************************/
/*  terrain_streamer.cpp                                                */
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

#include "terrain_streamer.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

void TerrainStreamer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_paused", "value"), &TerrainStreamer::set_paused);
	ClassDB::bind_method(D_METHOD("is_paused"), &TerrainStreamer::is_paused);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_paused", "is_paused");
	ClassDB::bind_method(D_METHOD("set_template_path", "value"), &TerrainStreamer::set_template_path);
	ClassDB::bind_method(D_METHOD("get_template_path"), &TerrainStreamer::get_template_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "template_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "TerrainView"), "set_template_path", "get_template_path");
	ClassDB::bind_method(D_METHOD("set_view_distance", "value"), &TerrainStreamer::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &TerrainStreamer::get_view_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "view_distance", PROPERTY_HINT_RANGE, "32,2048,16"), "set_view_distance", "get_view_distance");
	ClassDB::bind_method(D_METHOD("set_enabled", "value"), &TerrainStreamer::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &TerrainStreamer::is_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ClassDB::bind_method(D_METHOD("set_focus", "position", "direction"), &TerrainStreamer::set_focus, DEFVAL(Vector3()));
	ClassDB::bind_method(D_METHOD("rebuild"), &TerrainStreamer::rebuild);
	ClassDB::bind_method(D_METHOD("get_statistics"), &TerrainStreamer::get_statistics);
	ClassDB::bind_method(D_METHOD("sample_field", "position"), &TerrainStreamer::sample_field);
	ClassDB::bind_method(D_METHOD("has_terrain_at", "position"), &TerrainStreamer::has_terrain_at);
	ADD_SIGNAL(MethodInfo("stream_updated"));
}
TerrainView *TerrainStreamer::get_template() const {
	return Object::cast_to<TerrainView>(get_node_or_null(template_path));
}
void TerrainStreamer::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) {
		started_usec = OS::get_singleton()->get_ticks_usec();
		set_process_internal(true);
	} else if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
		tick();
	}
}
void TerrainStreamer::set_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}
	enabled = p_enabled;
	rebuild();
}
void TerrainStreamer::set_view_distance(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 32 || p_value > 2048);
	if (view_distance != p_value) {
		view_distance = p_value;
		dirty = true;
	}
}
void TerrainStreamer::set_focus(const Vector3 &p_position, const Vector3 &p_direction) {
	ERR_FAIL_COND(!p_position.is_finite() || p_position.abs().length() > 96000 || !p_direction.is_finite());
	focus = p_position;
	Vector2i cell(Math::floor(focus.x / 16), Math::floor(focus.z / 16));
	const Vector3 direction = Vector3(p_direction.x, 0, p_direction.z).normalized();
	if (cell != focus_cell || movement.dot(direction) < 0.5) {
		focus_cell = cell;
		dirty = true;
	}
	if (!direction.is_zero_approx()) {
		movement = direction;
	}
}
void TerrainStreamer::retire_patch(TerrainView *p_view) {
	p_view->cancel_generation();
	p_view->set_visible(false);
	// Collision/mesh teardown is initiated for one retiring patch per frame.
	retiring.push_back(p_view);
}
void TerrainStreamer::rebuild() {
	++epoch;
	snapshot_ready = false;
	blueprint.operations.clear();
	error = String();
	if (active) {
		retire_patch(active);
		active = nullptr;
	}
	invalidate_resident = enabled;
	if (!enabled) {
		for (const KeyValue<Vector2i, TerrainView *> &entry : resident) {
			retire_patch(entry.value);
		}
		resident.clear();
		resident_triangles = 0;
	}
	queue.clear();
	wanted.clear();
	dirty = true;
	started_usec = OS::get_singleton()->get_ticks_usec();
}
real_t TerrainStreamer::score(const Vector2i &p_cell) const {
	const Vector3 offset((p_cell.x + 0.5) * patch_size - focus.x, 0, (p_cell.y + 0.5) * patch_size - focus.z);
	// Distance dominates; forward preference breaks ties within nearby rings.
	return offset.length() - offset.normalized().dot(movement) * patch_size * 0.35;
}
void TerrainStreamer::plan() {
	dirty = false;
	queue.clear();
	wanted.clear();
	TerrainView *source = get_template();
	if (!source) {
		error = "template_path must identify a TerrainView.";
		return;
	}
	RegionContainer *root = Object::cast_to<RegionContainer>(source->get_node_or_null(source->get_root_path()));
	if (!root) {
		error = "Terrain template has no root RegionContainer.";
		return;
	}
	const int min_x = Math::floor((focus.x - view_distance) / patch_size);
	const int max_x = Math::floor((focus.x + view_distance) / patch_size);
	const int min_z = Math::floor((focus.z - view_distance) / patch_size);
	const int max_z = Math::floor((focus.z + view_distance) / patch_size);

	if (!snapshot_ready) {
		if (!root->collect_operations(blueprint.operations, error)) {
			return;
		}
		blueprint.bounds = source->get_preview_area();
		blueprint.spacing = source->get_sample_spacing();
		blueprint.section_cells = source->get_section_cells();
		blueprint.sample_budget = source->get_sample_budget();
		blueprint.triangle_budget = source->get_triangle_budget();
		snapshot_workers = source->get_worker_count();
		snapshot_collision = source->is_collision_enabled();
		snapshot_lod = source->is_lod_enabled();
		snapshot_lod_distance = source->get_lod_distance();
		snapshot_lod_error = source->get_lod_error();
		snapshot_lod_near_error = source->get_lod_near_error();
		snapshot_lod_max_level = source->get_lod_max_level();
		snapshot_bounded = root->is_bounded();
		snapshot_area = AABB(root->get_area().position + root->get_global_position(), root->get_area().size);

		snapshot_ready = true;
	}
	if (invalidate_resident) {
		// Validate the new authoring program before retiring a healthy epoch.
		for (const KeyValue<Vector2i, TerrainView *> &entry : resident) {
			retire_patch(entry.value);
		}
		resident.clear();
		resident_triangles = 0;
		invalidate_resident = false;
	}
	const AABB authored = blueprint.bounds;
	const AABB root_area = snapshot_area;
	for (int z = min_z; z <= max_z; ++z) {
		for (int x = min_x; x <= max_x; ++x) {
			const real_t dx = MAX(real_t(0), Math::abs((x + 0.5) * patch_size - focus.x) - patch_size * 0.5);
			const real_t dz = MAX(real_t(0), Math::abs((z + 0.5) * patch_size - focus.z) - patch_size * 0.5);
			if (dx * dx + dz * dz > view_distance * view_distance) {
				continue;
			}
			const Vector2i cell(x, z);
			const AABB area(Vector3(x * patch_size, authored.position.y, z * patch_size), Vector3(patch_size, authored.size.y, patch_size));
			if (snapshot_bounded && !root_area.intersects(area)) {
				continue;
			}
			wanted.insert(cell);
			bool refresh_lod = false;
			if (resident.has(cell) && snapshot_lod) {
				const Vector3 center((x + 0.5) * patch_size, 0, (z + 0.5) * patch_size);
				const Vector3 prior = resident[cell]->get_lod_origin() * Vector3(1, 0, 1);
				const real_t near = snapshot_lod_distance + patch_size;
				const real_t current_distance = center.distance_to(focus * Vector3(1, 0, 1));
				const real_t prior_distance = center.distance_to(prior);
				refresh_lod = (current_distance < near && prior_distance > near * 1.5) || (current_distance > near * 3 && prior_distance < near * 1.5);
			}
			if ((!resident.has(cell) || refresh_lod) && (!active || cell != active_cell)) {
				queue.push_back(cell);
			}
		}
	}
	Vector<Vector2i> removed;
	for (const KeyValue<Vector2i, TerrainView *> &entry : resident) {
		if (!wanted.has(entry.key)) {
			resident_triangles -= int64_t(entry.value->get_statistics()["triangles"]);
			retire_patch(entry.value);
			removed.push_back(entry.key);
		}
	}
	for (const Vector2i &cell : removed) {
		resident.erase(cell);
	}
	// Stable coordinate tie-break makes stationary startup reproducible.
}
void TerrainStreamer::tick() {
	// At most one retired patch per frame, after its coordinator has exited.
	// Its TerrainView also budgets individual mesh/physics retirement internally.
	if (!retiring.is_empty()) {
		TerrainView *view = retiring[0];
		if (!view->is_generating()) {
			view->retire_preview();
			if (!view->has_pending_retirement()) {
				retiring.remove_at(0);
				remove_child(view);
				memdelete(view);
			}
		}
	}
	if (!enabled || paused || Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (dirty) {
		plan();
	}
	if (active) {
		real_t nearest = 1e30;
		for (const Vector2i &cell : queue) {
			nearest = MIN(nearest, score(cell));
		}
		if (!wanted.has(active_cell) || (score(active_cell) > patch_size * 2 && nearest < patch_size)) {
			retire_patch(active);
			active = nullptr;
			dirty = true;
		}
		return;
	}
	// Never overlap a cancelled worker batch with its replacement.
	for (TerrainView *view : retiring) {
		if (view->is_generating()) {
			return;
		}
	}
	if (queue.is_empty() || OS::get_singleton()->get_ticks_usec() < next_attempt_usec) {
		return;
	}
	TerrainView *source = get_template();
	if (!source) {
		return;
	}
	int best = 0;
	for (int i = 1; i < queue.size(); ++i) {
		if (score(queue[i]) < score(queue[best])) {
			best = i;
		}
	}
	if (resident_triangles >= blueprint.triangle_budget && !resident.has(queue[best])) {
		Vector2i farthest;
		real_t worst = score(queue[best]);
		bool found = false;
		for (const KeyValue<Vector2i, TerrainView *> &entry : resident) {
			if (score(entry.key) > worst) {
				worst = score(entry.key);
				farthest = entry.key;
				found = true;
			}
		}
		if (!found) {
			error = "Resident triangle budget reached; nearer terrain retained.";
			return;
		}
		resident_triangles -= int64_t(resident[farthest]->get_statistics()["triangles"]);
		retire_patch(resident[farthest]);
		resident.erase(farthest);
	}
	active_cell = queue[best];
	queue.remove_at(best);
	RegionContainer *root = Object::cast_to<RegionContainer>(source->get_node_or_null(source->get_root_path()));
	if (!root) {
		return;
	}
	active = memnew(TerrainView);
	active->set_auto_generate(false);
	active->set_sample_spacing(blueprint.spacing);
	active->set_section_cells(blueprint.section_cells);
	active->set_worker_count(snapshot_workers);
	active->set_collision_enabled(snapshot_collision);
	active->set_sample_budget(blueprint.sample_budget);
	active->set_triangle_budget(blueprint.triangle_budget);
	active->set_publication_budget_ms(1.0);
	active->set_lod_enabled(snapshot_lod);
	active->set_lod_origin(focus);
	active->set_lod_distance(snapshot_lod_distance);
	active->set_lod_error(snapshot_lod_error);
	active->set_lod_near_error(snapshot_lod_near_error);
	active->set_lod_max_level(snapshot_lod_max_level);
	const AABB area = blueprint.bounds;
	active->set_preview_area(AABB(Vector3(active_cell.x * patch_size, area.position.y, active_cell.y * patch_size), Vector3(patch_size, area.size.y, patch_size)));
	add_child(active);
	active->set_root_path(active->get_path_to(root));
	active->connect("preview_generated", callable_mp(this, &TerrainStreamer::patch_completed));
	active->connect("generation_failed", callable_mp(this, &TerrainStreamer::patch_failed));
	active->set_program_snapshot(blueprint.operations, snapshot_bounded, snapshot_area);
	active->generate_preview();
}
void TerrainStreamer::patch_completed(const Dictionary &p_stats) {
	if (!active) {
		return;
	}
	const int64_t triangles = p_stats["triangles"];
	const TerrainView *source = get_template();
	const int64_t previous_triangles = resident.has(active_cell) ? int64_t(resident[active_cell]->get_statistics()["triangles"]) : 0;
	while (source && resident_triangles - previous_triangles + triangles > blueprint.triangle_budget) {
		Vector2i farthest;
		real_t worst = score(active_cell);
		bool found = false;
		for (const KeyValue<Vector2i, TerrainView *> &entry : resident) {
			if (entry.key != active_cell && score(entry.key) > worst) {
				worst = score(entry.key);
				farthest = entry.key;
				found = true;
			}
		}
		if (!found) {
			break;
		}
		resident_triangles -= int64_t(resident[farthest]->get_statistics()["triangles"]);
		retire_patch(resident[farthest]);
		resident.erase(farthest);
	}

	if (!wanted.has(active_cell) || (source && resident_triangles - previous_triangles + triangles > blueprint.triangle_budget)) {
		retire_patch(active);
		if (wanted.has(active_cell)) {
			error = "Resident triangle budget reached.";
		}
	} else {
		if (resident.has(active_cell)) {
			retire_patch(resident[active_cell]);
		}
		resident_triangles -= previous_triangles;
		error = String();
		active->release_sample_cache();
		resident.insert(active_cell, active);
		resident_triangles += triangles;
	}
	active = nullptr;
	emit_signal("stream_updated");
}
void TerrainStreamer::patch_failed(const String &p_error) {
	error = p_error;
	if (active) {
		retire_patch(active);
		active = nullptr;
	}
	// Keep the failed cell out of this plan; explicit rebuild retries it.
	next_attempt_usec = OS::get_singleton()->get_ticks_usec() + 100000;
	emit_signal("stream_updated");
}
Dictionary TerrainStreamer::get_statistics() const {
	Dictionary stats;
	stats["revision"] = epoch;
	stats["resident_patches"] = resident.size();
	stats["wanted_patches"] = wanted.size();
	stats["queued_patches"] = queue.size();
	stats["active"] = active != nullptr;
	stats["paused"] = paused;
	stats["active_cell"] = active_cell;
	stats["active_priority"] = active ? score(active_cell) : 0;
	stats["focus"] = focus;
	stats["movement"] = movement;
	stats["triangles"] = resident_triangles;
	stats["error"] = error;
	stats["bounds"] = AABB(Vector3(focus.x - view_distance, snapshot_ready ? blueprint.bounds.position.y : -64, focus.z - view_distance), Vector3(view_distance * 2, snapshot_ready ? blueprint.bounds.size.y : 256, view_distance * 2));
	stats["effective_spacing"] = snapshot_ready ? blueprint.spacing : 1;
	stats["elapsed_ms"] = double(OS::get_singleton()->get_ticks_usec() - started_usec) / 1000;
	Array cells;
	for (const KeyValue<Vector2i, TerrainView *> &entry : resident) {
		cells.push_back(entry.key);
	}
	stats["resident_cells"] = cells;
	Array queued;
	for (const Vector2i &cell : queue) {
		queued.push_back(cell);
	}
	stats["queued_cells"] = queued;
	return stats;
}
bool TerrainStreamer::has_terrain_at(const Vector3 &p_position) const {
	return resident.has(Vector2i(Math::floor(p_position.x / patch_size), Math::floor(p_position.z / patch_size)));
}
float TerrainStreamer::sample_field(const Vector3 &p_position) const {
	const Vector2i cell(Math::floor(p_position.x / patch_size), Math::floor(p_position.z / patch_size));
	// The immutable program covers the entire world, even when sample arrays have
	// been released. Sampling is a diagnostic, not scene/physics generation.
	const TerrainView *result = resident.has(cell) ? resident[cell] : nullptr;
	if (!result && !resident.is_empty()) {
		result = resident.begin()->value;
	}
	return result ? result->sample_field(p_position) : p_position.y;
}
