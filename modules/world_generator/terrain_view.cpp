/**************************************************************************/
/*  terrain_view.cpp                                                      */
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

#include "terrain_view.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

void TerrainView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &TerrainView::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &TerrainView::get_root_path);
	ClassDB::bind_method(D_METHOD("set_preview_area", "area"), &TerrainView::set_preview_area);
	ClassDB::bind_method(D_METHOD("get_preview_area"), &TerrainView::get_preview_area);
	ClassDB::bind_method(D_METHOD("set_sample_spacing", "spacing"), &TerrainView::set_sample_spacing);
	ClassDB::bind_method(D_METHOD("get_sample_spacing"), &TerrainView::get_sample_spacing);
	ClassDB::bind_method(D_METHOD("set_chunk_cells", "cells"), &TerrainView::set_chunk_cells);
	ClassDB::bind_method(D_METHOD("get_chunk_cells"), &TerrainView::get_chunk_cells);
	ClassDB::bind_method(D_METHOD("set_auto_generate", "enabled"), &TerrainView::set_auto_generate);
	ClassDB::bind_method(D_METHOD("is_auto_generate"), &TerrainView::is_auto_generate);
	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &TerrainView::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("is_collision_enabled"), &TerrainView::is_collision_enabled);
	ClassDB::bind_method(D_METHOD("generate_preview"), &TerrainView::generate_preview);
	ClassDB::bind_method(D_METHOD("sample_field", "world_position"), &TerrainView::sample_field);
	ClassDB::bind_method(D_METHOD("get_statistics"), &TerrainView::get_statistics);
	ClassDB::bind_method(D_METHOD("get_status"), &TerrainView::get_status);
	ClassDB::bind_method(D_METHOD("get_generate_action"), &TerrainView::get_generate_action);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "RegionContainer"), "set_root_path", "get_root_path");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "preview_area"), "set_preview_area", "get_preview_area");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sample_spacing", PROPERTY_HINT_RANGE, "0.025,2,0.025"), "set_sample_spacing", "get_sample_spacing");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_cells", PROPERTY_HINT_RANGE, "4,64,1"), "set_chunk_cells", "get_chunk_cells");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_generate"), "set_auto_generate", "is_auto_generate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "is_collision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "generate_action", PROPERTY_HINT_TOOL_BUTTON, "Generate Preview,Reload", PROPERTY_USAGE_EDITOR), "", "get_generate_action");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "status", PROPERTY_HINT_MULTILINE_TEXT, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_status");
	ADD_SIGNAL(MethodInfo("preview_generated", PropertyInfo(Variant::DICTIONARY, "statistics")));
}

void TerrainView::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY && auto_generate && !Engine::get_singleton()->is_editor_hint()) {
		call_deferred(SNAME("generate_preview"));
	}
}

void TerrainView::set_root_path(const NodePath &p_path) {
	root_path = p_path;
}
NodePath TerrainView::get_root_path() const {
	return root_path;
}
void TerrainView::set_preview_area(const AABB &p_area) {
	preview_area = p_area;
}
AABB TerrainView::get_preview_area() const {
	return preview_area;
}
void TerrainView::set_sample_spacing(real_t p_spacing) {
	sample_spacing = p_spacing;
}
real_t TerrainView::get_sample_spacing() const {
	return sample_spacing;
}
void TerrainView::set_chunk_cells(int p_cells) {
	chunk_cells = p_cells;
}
int TerrainView::get_chunk_cells() const {
	return chunk_cells;
}
void TerrainView::set_auto_generate(bool p_enabled) {
	auto_generate = p_enabled;
}
bool TerrainView::is_auto_generate() const {
	return auto_generate;
}
void TerrainView::set_collision_enabled(bool p_enabled) {
	collision_enabled = p_enabled;
}
bool TerrainView::is_collision_enabled() const {
	return collision_enabled;
}
Callable TerrainView::get_generate_action() {
	return Callable(this, SNAME("generate_preview"));
}
Dictionary TerrainView::get_statistics() const {
	return statistics.duplicate();
}
String TerrainView::get_status() const {
	return status;
}

bool TerrainView::fail_generation(const String &p_error) {
	status = "Generation failed; previous preview retained. " + p_error;
	generating = false;
	notify_property_list_changed();
	return false;
}

bool TerrainView::generate_preview() {
	if (generating) {
		return false;
	}
	generating = true;
	const uint64_t started = OS::get_singleton()->get_ticks_usec();
	if (!is_inside_tree()) {
		return fail_generation("Add TerrainView to the scene tree first.");
	}
	// Data is world-space. Keep the view adapter neutral in this first slice.
	if (!get_global_transform().is_equal_approx(Transform3D())) {
		return fail_generation("TerrainView must have an identity global transform in this prototype.");
	}
	RegionContainer *root = Object::cast_to<RegionContainer>(get_node_or_null(root_path));
	if (!root) {
		return fail_generation("root_path must identify a RegionContainer.");
	}
	if (Object::cast_to<RegionContainer>(root->get_parent())) {
		return fail_generation("root_path must select the top region container, not a nested child.");
	}
	TerrainData candidate;
	candidate.spacing = sample_spacing;
	candidate.chunk_cells = chunk_cells;
	candidate.bounds = preview_area;
	String error;
	if (!root->collect_operations(candidate.operations, error)) {
		return fail_generation(error);
	}
	if (root->is_bounded()) {
		const AABB root_area(root->get_area().position + root->get_global_position(), root->get_area().size);
		candidate.bounds = candidate.bounds.intersection(root_area);
	}
	if (!candidate.build(error)) {
		return fail_generation(error);
	}
	Vector<TerrainSurface> surfaces;
	if (!TerrainMesher::build(candidate, surfaces, error)) {
		return fail_generation(error);
	}
	// All expensive candidate generation happens before the old preview is touched.
	Node3D *replacement = memnew(Node3D);
	replacement->set_name("GeneratedPreview");
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_roughness(0.9);
	int64_t triangle_count = 0;
	for (const TerrainSurface &surface : surfaces) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = surface.vertices;
		arrays[Mesh::ARRAY_NORMAL] = surface.normals;
		arrays[Mesh::ARRAY_COLOR] = surface.colors;
		Ref<ArrayMesh> mesh;
		mesh.instantiate();
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		mesh->surface_set_material(0, material);
		MeshInstance3D *instance = memnew(MeshInstance3D);
		instance->set_position(surface.origin);
		instance->set_mesh(mesh);
		replacement->add_child(instance);
		triangle_count += surface.vertices.size() / 3;
		if (collision_enabled) {
			Ref<ConcavePolygonShape3D> shape = mesh->create_trimesh_shape();
			if (shape.is_null()) {
				memdelete(replacement);
				return fail_generation("Could not prepare triangle collision.");
			}
			StaticBody3D *body = memnew(StaticBody3D);
			CollisionShape3D *collision = memnew(CollisionShape3D);
			collision->set_shape(shape);
			body->add_child(collision);
			instance->add_child(body);
		}
	}
	// Synchronous editor preview publication only. Production physics-tick
	// synchronization, readers, persistence and concurrent edits are not implemented.
	if (generated) {
		remove_child(generated);
		memdelete(generated);
	}
	generated = replacement;
	add_child(generated);
	published_data = candidate;
	++revision;
	statistics.clear();
	statistics["revision"] = revision;
	statistics["chunk_count"] = candidate.chunks.size();
	statistics["uniform_chunks"] = candidate.get_uniform_chunk_count();
	statistics["sample_payload_bytes"] = candidate.get_payload_bytes();
	statistics["mesh_chunks"] = surfaces.size();
	statistics["triangles"] = triangle_count;
	statistics["bounds"] = candidate.bounds;
	statistics["generation_ms"] = double(OS::get_singleton()->get_ticks_usec() - started) / 1000.0;
	status = vformat("Revision %d: %d chunks (%d uniform), %d triangles, %.1f ms", revision, candidate.chunks.size(), candidate.get_uniform_chunk_count(), triangle_count, double(statistics["generation_ms"]));
	generating = false;
	notify_property_list_changed();
	emit_signal(SNAME("preview_generated"), statistics.duplicate());
	return true;
}

float TerrainView::sample_field(const Vector3 &p_world_position) const {
	ERR_FAIL_COND_V(revision == 0 || !p_world_position.is_finite(), 0);
	// Inspect the published compiled field, not resources edited since generation.
	return published_data.sample(p_world_position).field;
}
