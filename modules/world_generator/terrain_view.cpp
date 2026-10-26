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

void TerrainView::set_lod_origin(const Vector3 &p_value) {
	ERR_FAIL_COND(!p_value.is_finite());
	lod_origin = p_value;
}
void TerrainView::set_lod_distance(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 4 || p_value > 100000);
	lod_distance = p_value;
}
void TerrainView::set_lod_max_level(int p_value) {
	ERR_FAIL_COND(p_value < 1 || p_value > 4);
	lod_max_level = p_value;
}
void TerrainView::set_lod_near_error(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0 || p_value > 1);
	lod_near_error = p_value;
}
void TerrainView::set_lod_error(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.01 || p_value > 2);
	lod_error = p_value;
}

void TerrainView::set_heightmap_stride(int p_stride) {
	ERR_FAIL_COND(p_stride < 1 || p_stride > 32);
	heightmap_stride = p_stride;
}
void TerrainView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_lod_max_level", "value"), &TerrainView::set_lod_max_level);
	ClassDB::bind_method(D_METHOD("get_lod_max_level"), &TerrainView::get_lod_max_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_max_level", PROPERTY_HINT_RANGE, "1,4,1"), "set_lod_max_level", "get_lod_max_level");
	ClassDB::bind_method(D_METHOD("set_lod_near_error", "value"), &TerrainView::set_lod_near_error);
	ClassDB::bind_method(D_METHOD("get_lod_near_error"), &TerrainView::get_lod_near_error);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_near_error", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_lod_near_error", "get_lod_near_error");
	ClassDB::bind_method(D_METHOD("set_heightmap_csv_path", "path"), &TerrainView::set_heightmap_csv_path);
	ClassDB::bind_method(D_METHOD("get_heightmap_csv_path"), &TerrainView::get_heightmap_csv_path);
	ClassDB::bind_method(D_METHOD("set_heightmap_stride", "stride"), &TerrainView::set_heightmap_stride);
	ClassDB::bind_method(D_METHOD("get_heightmap_stride"), &TerrainView::get_heightmap_stride);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "heightmap_csv_path", PROPERTY_HINT_SAVE_FILE, "*.csv"), "set_heightmap_csv_path", "get_heightmap_csv_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "heightmap_stride", PROPERTY_HINT_RANGE, "1,32,1"), "set_heightmap_stride", "get_heightmap_stride");
	ClassDB::bind_method(D_METHOD("set_lod_enabled", "value"), &TerrainView::set_lod_enabled);
	ClassDB::bind_method(D_METHOD("is_lod_enabled"), &TerrainView::is_lod_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lod_enabled"), "set_lod_enabled", "is_lod_enabled");
	ClassDB::bind_method(D_METHOD("set_lod_origin", "value"), &TerrainView::set_lod_origin);
	ClassDB::bind_method(D_METHOD("get_lod_origin"), &TerrainView::get_lod_origin);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "lod_origin"), "set_lod_origin", "get_lod_origin");
	ClassDB::bind_method(D_METHOD("set_lod_distance", "value"), &TerrainView::set_lod_distance);
	ClassDB::bind_method(D_METHOD("get_lod_distance"), &TerrainView::get_lod_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_distance", PROPERTY_HINT_RANGE, "4,256,1,or_greater"), "set_lod_distance", "get_lod_distance");
	ClassDB::bind_method(D_METHOD("set_lod_error", "value"), &TerrainView::set_lod_error);
	ClassDB::bind_method(D_METHOD("get_lod_error"), &TerrainView::get_lod_error);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_error", PROPERTY_HINT_RANGE, "0.01,2,0.01"), "set_lod_error", "get_lod_error");

	ClassDB::bind_method(D_METHOD("set_worker_count", "count"), &TerrainView::set_worker_count);
	ClassDB::bind_method(D_METHOD("get_worker_count"), &TerrainView::get_worker_count);
	ClassDB::bind_method(D_METHOD("set_sample_budget", "samples"), &TerrainView::set_sample_budget);
	ClassDB::bind_method(D_METHOD("get_sample_budget"), &TerrainView::get_sample_budget);
	ClassDB::bind_method(D_METHOD("set_triangle_budget", "triangles"), &TerrainView::set_triangle_budget);
	ClassDB::bind_method(D_METHOD("get_triangle_budget"), &TerrainView::get_triangle_budget);
	ClassDB::bind_method(D_METHOD("set_publication_budget_ms", "milliseconds"), &TerrainView::set_publication_budget_ms);
	ClassDB::bind_method(D_METHOD("get_publication_budget_ms"), &TerrainView::get_publication_budget_ms);
	ClassDB::bind_method(D_METHOD("is_generating"), &TerrainView::is_generating);
	ClassDB::bind_method(D_METHOD("get_progress"), &TerrainView::get_progress);
	ClassDB::bind_method(D_METHOD("cancel_generation"), &TerrainView::cancel_generation);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "worker_count", PROPERTY_HINT_RANGE, "1,256,1,or_greater"), "set_worker_count", "get_worker_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sample_budget", PROPERTY_HINT_RANGE, "1000000,64000000,1000000"), "set_sample_budget", "get_sample_budget");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "triangle_budget", PROPERTY_HINT_RANGE, "10000,4000000,10000"), "set_triangle_budget", "get_triangle_budget");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "publication_budget_ms", PROPERTY_HINT_RANGE, "1,16,0.5"), "set_publication_budget_ms", "get_publication_budget_ms");
	ADD_SIGNAL(MethodInfo("generation_started", PropertyInfo(Variant::INT, "request_id")));
	ADD_SIGNAL(MethodInfo("generation_failed", PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("generation_cancelled"));
	ClassDB::bind_method(D_METHOD("set_section_cells", "cells"), &TerrainView::set_section_cells);
	ClassDB::bind_method(D_METHOD("get_section_cells"), &TerrainView::get_section_cells);
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
	ADD_PROPERTY(PropertyInfo(Variant::INT, "section_cells", PROPERTY_HINT_RANGE, "4,64,1"), "set_section_cells", "get_section_cells");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_cells", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL), "set_chunk_cells", "get_chunk_cells");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_generate"), "set_auto_generate", "is_auto_generate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "is_collision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::CALLABLE, "generate_action", PROPERTY_HINT_TOOL_BUTTON, "Generate Preview,Reload", PROPERTY_USAGE_EDITOR), "", "get_generate_action");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "status", PROPERTY_HINT_MULTILINE_TEXT, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_status");
	ADD_SIGNAL(MethodInfo("preview_generated", PropertyInfo(Variant::DICTIONARY, "statistics")));
}

void TerrainView::_notification(int p_what) {
	if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
		process_generation();
	}
	if (p_what == NOTIFICATION_EXIT_TREE) {
		cancel_generation();
	}
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
void TerrainView::set_section_cells(int p_cells) {
	section_cells = p_cells;
}
int TerrainView::get_section_cells() const {
	return section_cells;
}
void TerrainView::set_chunk_cells(int p_cells) {
	// Serialized compatibility with the initial variable-size chunk prototype.
	set_section_cells(p_cells);
}
int TerrainView::get_chunk_cells() const {
	return get_section_cells();
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

TerrainView::TerrainView() {
	worker_count = MAX(1, OS::get_singleton()->get_processor_count());
}

void TerrainView::set_worker_count(int p_count) {
	ERR_FAIL_COND(p_count < 1);
	worker_count = p_count;
}
int TerrainView::get_worker_count() const {
	return worker_count;
}
void TerrainView::set_sample_budget(int64_t p_samples) {
	ERR_FAIL_COND(p_samples < 1000000 || p_samples > 64000000);
	sample_budget = p_samples;
}
int64_t TerrainView::get_sample_budget() const {
	return sample_budget;
}
void TerrainView::set_triangle_budget(int64_t p_triangles) {
	ERR_FAIL_COND(p_triangles < 10000 || p_triangles > 4000000);
	triangle_budget = p_triangles;
}
int64_t TerrainView::get_triangle_budget() const {
	return triangle_budget;
}
void TerrainView::set_publication_budget_ms(real_t p_ms) {
	ERR_FAIL_COND(!Math::is_finite(p_ms) || p_ms < 1 || p_ms > 16);
	publication_budget_ms = p_ms;
}
real_t TerrainView::get_publication_budget_ms() const {
	return publication_budget_ms;
}
bool TerrainView::is_generating() const {
	return job || pending;
}

Dictionary TerrainView::get_progress() const {
	Dictionary result;
	result["active"] = is_generating();
	result["stage"] = request_cancelled && job ? "Cancelling" : pending ? "Cancelling obsolete request"
																		: (staging ? "Publishing" : (job ? "Worker generation" : "Idle"));
	result["request_id"] = pending ? pending->request_id : (job ? job->request_id : next_request);
	result["completed_sections"] = job ? job->completed_sections.get() : 0;
	result["total_sections"] = job ? job->total_sections.get() : 0;
	result["published_pieces"] = publication_index;
	// Do not read the surfaces vector while the coordinator is writing it.
	result["total_pieces"] = job && job->done.is_set() ? job->surfaces.size() : 0;
	return result;
}

TerrainView::~TerrainView() {
	if (pending) {
		memdelete(pending);
	}
	// Workers own only job data, never this node. Join during destruction after
	// cooperative cancellation so no callbacks outlive their immutable snapshot.
	if (job) {
		memdelete(job);
	}
}

void TerrainView::cancel_generation() {
	request_cancelled = true;
	if (job) {
		job->cancelled.set();
	}
	if (pending) {
		memdelete(pending);
		pending = nullptr;
	}
	if (is_generating()) {
		status = "Cancelling; previous preview retained.";
		emit_signal(SNAME("generation_cancelled"));
	}
}

bool TerrainView::fail_generation(const String &p_error) {
	status = "Generation failed; previous preview retained. " + p_error;
	notify_property_list_changed();
	emit_signal(SNAME("generation_failed"), status);
	return false;
}

bool TerrainView::generate_preview() {
	ERR_FAIL_COND_V(!Thread::is_main_thread(), false);
#ifndef THREADS_ENABLED
	return fail_generation("This build has no worker-thread support.");
#else
	if (!is_inside_tree()) {
		return fail_generation("Add TerrainView to the scene tree first.");
	}
	if (!get_global_transform().is_equal_approx(Transform3D())) {
		return fail_generation("TerrainView must have an identity global transform.");
	}
	RegionContainer *root = Object::cast_to<RegionContainer>(get_node_or_null(root_path));
	if (!root || Object::cast_to<RegionContainer>(root->get_parent())) {
		return fail_generation("root_path must identify the top RegionContainer.");
	}
	TerrainBuildJob *candidate = memnew(TerrainBuildJob);
	candidate->started_usec = OS::get_singleton()->get_ticks_usec();
	candidate->data.spacing = sample_spacing;
	candidate->data.section_cells = section_cells;
	candidate->data.sample_budget = sample_budget;
	candidate->data.triangle_budget = triangle_budget;
	candidate->data.bounds = preview_area;
	candidate->heightmap_csv_path = heightmap_csv_path;
	candidate->heightmap_stride = heightmap_stride;
	candidate->workers = worker_count;
	candidate->collision = collision_enabled;
	candidate->lod_enabled = lod_enabled;
	candidate->lod_origin = lod_origin;
	candidate->lod_distance = lod_distance;
	candidate->lod_error = lod_error;
	candidate->lod_near_error = lod_near_error;
	candidate->lod_max_level = lod_max_level;
	String error;
	// Only authoring access happens here. Compilation captures immutable C++
	// values before the worker receives anything; no node/Resource reads on it.
	if (program_snapshot_enabled) {
		candidate->data.operations = program_snapshot;
	} else if (!root->collect_operations(candidate->data.operations, error)) {
		memdelete(candidate);
		return fail_generation(error);
	}
	if (program_snapshot_enabled ? program_snapshot_bounded : root->is_bounded()) {
		const AABB root_area = program_snapshot_enabled ? program_snapshot_area : AABB(root->get_area().position + root->get_global_position(), root->get_area().size);
		candidate->data.bounds = candidate->data.bounds.intersection(root_area);
	}
	candidate->layout_ms = double(OS::get_singleton()->get_ticks_usec() - candidate->started_usec) / 1000.0;
	candidate->request_id = ++next_request;
	request_cancelled = false;
	if (job) {
		job->cancelled.set();
		if (pending) {
			memdelete(pending);
		}
		pending = candidate;
	} else {
		start_job(candidate);
	}
	status = "Generation queued on worker threads.";
	set_process_internal(true);
	emit_signal(SNAME("generation_started"), candidate->request_id);
	return true;
#endif
}

void TerrainView::start_job(TerrainBuildJob *p_job) {
	job = p_job;
	publication_index = 0;
	publication_cpu_ms = 0;
	max_publication_step_ms = 0;
	if (!job->start()) {
		job->error = "Could not start terrain worker coordinator.";
		job->done.set();
	}
}

void TerrainView::retire(Node3D *p_root) {
	if (!p_root) {
		return;
	}
	p_root->set_visible(false);
	p_root->set_name("RetiringTerrain");
	for (int i = 0; i < p_root->get_child_count(); ++i) {
		Node *body = p_root->get_child(i)->get_child_count() > 0 ? p_root->get_child(i)->get_child(0) : nullptr;
		if (StaticBody3D *collider = Object::cast_to<StaticBody3D>(body)) {
			collider->set_collision_layer(0);
			collider->set_collision_mask(0);
		}
	}
	retiring.push_back(p_root);
}

void TerrainView::cleanup_retired() {
	const uint64_t started = OS::get_singleton()->get_ticks_usec();
	while (!retiring.is_empty() && OS::get_singleton()->get_ticks_usec() - started < 1000) {
		Node3D *root = retiring[retiring.size() - 1];
		if (root->get_child_count() > 0) {
			Node *child = root->get_child(root->get_child_count() - 1);
			root->remove_child(child);
			memdelete(child);
		} else {
			remove_child(root);
			memdelete(root);
			retiring.resize(retiring.size() - 1);
		}
	}
}

bool TerrainView::publish_piece(const PreparedTerrainSurface &p_surface) {
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = p_surface.vertices;
	arrays[Mesh::ARRAY_NORMAL] = p_surface.normals;
	arrays[Mesh::ARRAY_COLOR] = p_surface.colors;
	arrays[Mesh::ARRAY_INDEX] = p_surface.indices;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	mesh->surface_set_material(0, preview_material);
	MeshInstance3D *instance = memnew(MeshInstance3D);
	instance->set_position(p_surface.origin);
	instance->set_mesh(mesh);
	staging->add_child(instance);
	if (job->collision) {
		Ref<ConcavePolygonShape3D> shape;
		shape.instantiate();
		shape->set_faces(p_surface.collision_faces);
		StaticBody3D *body = memnew(StaticBody3D);
		body->set_collision_layer(0);
		body->set_collision_mask(0);
		CollisionShape3D *collision = memnew(CollisionShape3D);
		collision->set_shape(shape);
		body->add_child(collision);
		instance->add_child(body);
	}
	return true;
}

void TerrainView::process_generation() {
	cleanup_retired();
	if (!job) {
		set_process_internal(!retiring.is_empty());
		return;
	}
	if (!job->done.is_set()) {
		return;
	}
	if (job->coordinator.is_started()) {
		job->coordinator.wait_to_finish();
	}
	if (pending || request_cancelled || !job->error.is_empty() || job->cancelled.is_set()) {
		if (staging) {
			retire(staging);
			staging = nullptr;
		}
		if (!pending && !request_cancelled && !job->error.is_empty()) {
			fail_generation(job->error);
		} else if (!pending && !status.begins_with("Generation failed")) {
			status = "Cancelled; previous preview retained.";
		}
		memdelete(job);
		job = nullptr;
		if (pending) {
			TerrainBuildJob *next = pending;
			pending = nullptr;
			request_cancelled = false;
			start_job(next);
		}
		return;
	}
	if (!staging) {
		staging = memnew(Node3D);
		staging->set_name("PreparingTerrain");
		staging->set_visible(false);
		add_child(staging);
		Ref<StandardMaterial3D> material;
		material.instantiate();
		material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		material->set_roughness(0.9);
		preview_material = material;
	}
	const uint64_t started = OS::get_singleton()->get_ticks_usec();
	const uint64_t budget = uint64_t(publication_budget_ms * 1000);
	while (publication_index < job->surfaces.size()) {
		publish_piece(job->surfaces[publication_index++]);
		if (OS::get_singleton()->get_ticks_usec() - started >= budget) {
			break;
		}
	}
	const double elapsed = double(OS::get_singleton()->get_ticks_usec() - started) / 1000.0;
	publication_cpu_ms += elapsed;
	max_publication_step_ms = MAX(max_publication_step_ms, elapsed);
	if (publication_index == job->surfaces.size()) {
		finish_generation();
	}
}

void TerrainView::finish_generation() {
	const uint64_t started = OS::get_singleton()->get_ticks_usec();
	retire(generated);
	generated = staging;
	staging = nullptr;
	generated->set_name("GeneratedPreview");
	for (int i = 0; i < generated->get_child_count(); ++i) {
		Node *part = generated->get_child(i);
		if (part->get_child_count() > 0) {
			StaticBody3D *body = Object::cast_to<StaticBody3D>(part->get_child(0));
			if (body) {
				body->set_collision_layer(1);
				body->set_collision_mask(1);
			}
		}
	}
	generated->set_visible(true);
	published_data = job->data;
	++revision;
	statistics.clear();
	statistics["revision"] = revision;
	statistics["request_id"] = job->request_id;
	statistics["chunk_count"] = job->data.world_chunk_count;
	statistics["section_count"] = job->data.sections.size();
	statistics["chunk_size"] = TerrainData::WORLD_CHUNK_SIZE;
	statistics["effective_spacing"] = job->data.spacing;
	statistics["section_cells"] = job->data.section_cells;
	statistics["column_samples"] = job->data.column_samples;
	statistics["layout_ms"] = job->layout_ms;
	statistics["sampling_ms"] = job->sampling_cpu_ms;
	statistics["meshing_ms"] = job->meshing_cpu_ms;
	statistics["sampling_cpu_ms"] = job->sampling_cpu_ms;
	statistics["meshing_cpu_ms"] = job->meshing_cpu_ms;
	statistics["heightmap"] = job->heightmap_statistics;
	statistics["heightmap_error"] = job->heightmap_error;
	statistics["heightmap_ms"] = job->heightmap_ms;
	statistics["worker_ms"] = job->worker_ms;
	statistics["worker_limit"] = job->workers;
	statistics["worker_threads_used"] = job->threads_used;
	statistics["generation_on_main_thread"] = job->main_thread_violation.is_set();
	statistics["publication_ms"] = publication_cpu_ms + double(OS::get_singleton()->get_ticks_usec() - started) / 1000.0;
	statistics["max_publication_step_ms"] = max_publication_step_ms;
	statistics["uniform_sections"] = job->data.get_uniform_section_count();
	statistics["uniform_chunks"] = job->data.get_uniform_section_count();
	statistics["sample_payload_bytes"] = job->data.get_payload_bytes();
	statistics["mesh_sections"] = job->mesh_section_count;
	statistics["mesh_chunks"] = job->mesh_section_count;
	statistics["mesh_pieces"] = job->surfaces.size();
	statistics["triangles"] = job->triangles.get();
	statistics["source_triangles"] = job->source_triangles.get();
	statistics["lod_enabled"] = job->lod_enabled;
	statistics["lod_origin"] = job->lod_origin;
	statistics["lod_sections"] = job->simplified_sections.get();
	statistics["bounds"] = job->data.bounds;
	statistics["generation_ms"] = double(OS::get_singleton()->get_ticks_usec() - job->started_usec) / 1000.0;
	memdelete(job);
	job = nullptr;
	statistics["commit_ms"] = double(OS::get_singleton()->get_ticks_usec() - started) / 1000.0;
	statistics["max_publication_step_ms"] = MAX(max_publication_step_ms, double(statistics["commit_ms"]));
	status = vformat("Revision %d: %d chunks, %d triangles, %d worker threads, %.1f ms", revision, int64_t(statistics["chunk_count"]), int64_t(statistics["triangles"]), int(statistics["worker_threads_used"]), double(statistics["generation_ms"]));
	notify_property_list_changed();
	emit_signal(SNAME("preview_generated"), statistics.duplicate());
}

float TerrainView::sample_field(const Vector3 &p_world_position) const {
	ERR_FAIL_COND_V(revision == 0 || !p_world_position.is_finite(), 0);
	// Inspect the published compiled field, not resources edited since generation.
	return published_data.sample(p_world_position).field;
}

void TerrainView::retire_preview() {
	if (generated) {
		retire(generated);
		generated = nullptr;
		set_process_internal(true);
	}
}
