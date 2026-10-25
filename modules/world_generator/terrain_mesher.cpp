/**************************************************************************/
/*  terrain_mesher.cpp                                                    */
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

#include "terrain_data.h"

#include "core/math/math_funcs.h"

namespace {
// Fixed cube diagonal 0 -> 6 gives matching face diagonals across chunks.
constexpr int TETRAHEDRA[6][4] = { { 0, 5, 1, 6 }, { 0, 1, 2, 6 }, { 0, 2, 3, 6 }, { 0, 3, 7, 6 }, { 0, 7, 4, 6 }, { 0, 4, 5, 6 } };
constexpr int EDGES[6][2] = { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 2 }, { 1, 3 }, { 2, 3 } };
const Vector3i CORNERS[8] = { Vector3i(0, 0, 0), Vector3i(1, 0, 0), Vector3i(1, 1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, 1), Vector3i(1, 0, 1), Vector3i(1, 1, 1), Vector3i(0, 1, 1) };
constexpr int64_t MAX_VERTICES = 1500000;

Color terrain_color(uint16_t p_material, const Vector3 &p_normal) {
	Color color;
	switch (p_material) {
		case 2:
			color = Color(0.42, 0.55, 0.29);
			break;
		case 3:
			color = Color(0.56, 0.60, 0.65);
			break;
		default:
			color = Color(0.48, 0.34, 0.22);
			break;
	}
	// Preview palette only; slope tint does not change the physical material ID.
	return color.lerp(Color(0.29, 0.35, 0.22), MAX(real_t(0), p_normal.y) * 0.3);
}

bool emit_triangle(const TerrainData &p_data, TerrainSurface &r_surface, Vector3 p_a, Vector3 p_b, Vector3 p_c, const Vector3 &p_surface_normal, int64_t &r_vertices) {
	Vector3 cross = (p_b - p_a).cross(p_c - p_a);
	if (cross.length_squared() < 1e-15) {
		return true;
	}
	// Godot front faces use clockwise winding; normals still point out of solid.
	if (cross.dot(p_surface_normal) > 0) {
		SWAP(p_b, p_c);
	}
	if (r_vertices + 3 > MAX_VERTICES) {
		return false;
	}
	for (const Vector3 &point : { p_a, p_b, p_c }) {
		const Vector3 vertex_normal = p_data.normal(point);
		const uint16_t material = p_data.sample(point - vertex_normal * p_data.spacing * 0.1).material;
		r_surface.vertices.push_back(point - r_surface.origin);
		r_surface.normals.push_back(vertex_normal);
		r_surface.colors.push_back(terrain_color(material, vertex_normal));
	}
	r_vertices += 3;
	return true;
}

bool clip_and_emit(const TerrainData &p_data, TerrainSurface &r_surface, const Vector3 &p_a, const Vector3 &p_b, const Vector3 &p_c, const Vector3 &p_surface_normal, int64_t &r_vertices) {
	// Clip output to the precise requested/root box, not an expanded chunk box.
	Vector3 polygon[12] = { p_a, p_b, p_c };
	int count = 3;
	for (int plane = 0; plane < 6 && count > 0; ++plane) {
		const int axis = plane / 2;
		const bool upper = plane % 2;
		const real_t limit = upper ? p_data.bounds.get_end()[axis] : p_data.bounds.position[axis];
		Vector3 output[12];
		int output_count = 0;
		for (int i = 0; i < count; ++i) {
			const Vector3 a = polygon[i];
			const Vector3 b = polygon[(i + 1) % count];
			const bool a_inside = upper ? a[axis] <= limit : a[axis] >= limit;
			const bool b_inside = upper ? b[axis] <= limit : b[axis] >= limit;
			if (a_inside) {
				output[output_count++] = a;
			}
			if (a_inside != b_inside) {
				output[output_count++] = a.lerp(b, (limit - a[axis]) / (b[axis] - a[axis]));
			}
		}
		count = output_count;
		for (int i = 0; i < count; ++i) {
			polygon[i] = output[i];
		}
	}
	for (int i = 1; i + 1 < count; ++i) {
		if (!emit_triangle(p_data, r_surface, polygon[0], polygon[i], polygon[i + 1], p_surface_normal, r_vertices)) {
			return false;
		}
	}
	return true;
}

bool polygonize_tetrahedron(const TerrainData &p_data, TerrainSurface &r_surface, const Vector3 *p_points, const float *p_values, const int *p_tetrahedron, int64_t &r_vertices) {
	Vector3 intersections[4];
	int count = 0;
	for (int edge = 0; edge < 6; ++edge) {
		int a = p_tetrahedron[EDGES[edge][0]];
		int b = p_tetrahedron[EDGES[edge][1]];
		if ((p_values[a] < 0) == (p_values[b] < 0)) {
			continue;
		}
		// Canonical edge direction gives identical intersections in adjacent tetrahedra.
		if (p_points[b] < p_points[a]) {
			SWAP(a, b);
		}
		const real_t weight = p_values[a] / (p_values[a] - p_values[b]);
		const Vector3 point = p_values[a] == 0 ? p_points[a] : (p_values[b] == 0 ? p_points[b] : p_points[a].lerp(p_points[b], weight));
		bool duplicate = false;
		for (int i = 0; i < count; ++i) {
			duplicate |= intersections[i].distance_squared_to(point) < 1e-14;
		}
		if (!duplicate) {
			intersections[count++] = point;
		}
	}
	if (count < 3) {
		return true;
	}
	Vector3 center;
	for (int i = 0; i < count; ++i) {
		center += intersections[i];
	}
	center /= count;
	// The polygon lies on the piecewise-linear sampled field. Its orientation must
	// use that field's exact gradient, not a shading normal from the analytic CSG
	// field: those can disagree at a sharp union/subtraction crease.
	const int a = p_tetrahedron[0];
	const int b = p_tetrahedron[1];
	const int c = p_tetrahedron[2];
	const int d = p_tetrahedron[3];
	const Vector3 ab = p_points[b] - p_points[a];
	const Vector3 ac = p_points[c] - p_points[a];
	const Vector3 ad = p_points[d] - p_points[a];
	const Vector3 gradient = ((p_values[b] - p_values[a]) * ac.cross(ad) +
									 (p_values[c] - p_values[a]) * ad.cross(ab) + (p_values[d] - p_values[a]) * ab.cross(ac)) /
			ab.dot(ac.cross(ad));
	const Vector3 normal = gradient.normalized();
	const Vector3 axis = (intersections[0] - center).normalized();
	const Vector3 tangent = normal.cross(axis).normalized();
	real_t angles[4];
	for (int i = 0; i < count; ++i) {
		const Vector3 delta = intersections[i] - center;
		angles[i] = Math::atan2(delta.dot(tangent), delta.dot(axis));
	}
	for (int i = 1; i < count; ++i) {
		for (int j = i; j > 0 && angles[j] < angles[j - 1]; --j) {
			SWAP(angles[j], angles[j - 1]);
			SWAP(intersections[j], intersections[j - 1]);
		}
	}
	for (int i = 1; i + 1 < count; ++i) {
		if (!clip_and_emit(p_data, r_surface, intersections[0], intersections[i], intersections[i + 1], normal, r_vertices)) {
			return false;
		}
	}
	return true;
}
} // namespace

bool TerrainMesher::build(const TerrainData &p_data, Vector<TerrainSurface> &r_surfaces, String &r_error) {
	r_surfaces.clear();
	int64_t vertices = 0;
	for (const TerrainChunk &chunk : p_data.chunks) {
		if (chunk.uniform) {
			continue;
		}
		TerrainSurface surface;
		surface.origin = chunk.origin;
		for (int z = 0; z < chunk.cells; ++z) {
			for (int y = 0; y < chunk.cells; ++y) {
				for (int x = 0; x < chunk.cells; ++x) {
					const Vector3i cell = chunk.address * chunk.cells + Vector3i(x, y, z);
					const Vector3 origin = Vector3(cell) * p_data.spacing;
					if (!AABB(origin, Vector3(p_data.spacing, p_data.spacing, p_data.spacing)).intersects(p_data.bounds)) {
						continue;
					}
					Vector3 points[8];
					float values[8];
					int solid_count = 0;
					for (int i = 0; i < 8; ++i) {
						points[i] = Vector3(cell + CORNERS[i]) * p_data.spacing;
						values[i] = chunk.field_at(x + CORNERS[i].x, y + CORNERS[i].y, z + CORNERS[i].z);
						// Values at mathematical zero can differ by a few float ulps.
						// Snap consistently before classification to avoid micro-slivers.
						if (Math::abs(values[i]) <= p_data.spacing * 0.00001) {
							values[i] = 0;
						}
						solid_count += values[i] < 0;
					}
					if (solid_count == 0 || solid_count == 8) {
						continue;
					}
					for (int i = 0; i < 6; ++i) {
						if (!polygonize_tetrahedron(p_data, surface, points, values, TETRAHEDRA[i], vertices)) {
							r_error = "Preview triangle budget exceeded (500000 triangles).";
							return false;
						}
					}
				}
			}
		}
		if (!surface.vertices.is_empty()) {
			r_surfaces.push_back(surface);
		}
	}
	return true;
}
