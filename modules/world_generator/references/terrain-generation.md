# Terrain generation references

## Red Blob Games — reviewed 2026-10-27

- [Terrain from noise](https://www.redblobgames.com/maps/terrain-from-noise/): use broad elevation/climate controls rather than selecting every biome independently. Demo010 expresses this through real nested Land/Oceans, climate-family, biome and formation containers. These are configurable authored families; there is no physical temperature/rainfall simulation yet.
- [Noisy edges](https://www.redblobgames.com/maps/noisy-edges/): preserve endpoints and connectivity when making graph boundaries irregular. The drainage planner uses a finer, world-anchored Voronoi dual graph with bounded endpoint-preserving edge displacement. It retries straight edges when terrain checks reject the noisy course. This is inspired by the principle, not an implementation of the article's recursive quadrilateral algorithm.
- [Mapgen4](https://www.redblobgames.com/maps/mapgen4/): terrain, climate/moisture and drainage influence one another. Our current increment samples existing terrain, identifies boundary-connected ocean outlets and routes highland headwaters toward them. Rainfall, water discharge, erosion and globally consistent catchments remain future work.
- [Voronoi maps tutorial](https://www.redblobgames.com/x/2022-voronoi-maps-tutorial/): graph connectivity is useful for routing and neighborhood queries. Native code uses Godot's Delaunay triangulation to obtain its dual drainage graph.

## What is deliberately separate

Terrain remains a continuous three-dimensional scalar field with material IDs, scoped decorators, caves and chunk meshing. A sampled top-surface projection is used only to plan drainage and evaluate terrain. The routing study is finite and independent of camera movement; moving beyond it does not create new catchments automatically. Routes do not yet follow the exact climate/biome borders: those operate at different scales.

Coastal lake candidates target Y=0 and proximity to ocean. Highland candidates use sampled local elevation and avoid existing river courses. Lakes are basin decorators, not simulated watershed filling. Water surfaces are optional static render aids.
