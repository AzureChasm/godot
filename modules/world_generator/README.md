# World Generator Terrain Prototype

This is the first reviewable C++ module prototype on `feature/world-generator-prototype`. It demonstrates the container/decorator authoring model and smooth three-dimensional terrain. The user reviewed the working prototype and its initial corrections and authorized this initial implementation commit.

## Build and open

From the Godot repository root:

```sh
scons platform=linuxbsd target=editor dev_build=yes tests=yes compiledb=yes -j16
./bin/godot.linuxbsd.editor.dev.x86_64 --editor --path modules/world_generator/demo
```

The demo is a standalone Godot project inside this module; it has no dependency on Astray. Terrain generates automatically only when running the scene (F6) or project (F5), not when opening the scene in the editor. The Inspector action can still generate an explicit editor preview. Right-drag orbits, the mouse wheel zooms, R regenerates, and Space toggles root boundedness while keeping the same finite view request.

The initial scene contains a hill with a carved cavity, a floating solid in a translated child container, and another child-scoped mound. The overlay reports generation time, chunks, compressed uniform chunks, and triangles. These are prototype measurements, not production performance guarantees.

## Edit the prototype

1. Select `World`. Only containers are children of this node. Its **Decorators** property is a list of `WorldDecorator` resources.
2. Expand the sphere resources and change their center, radius, operation, material ID, or priority. Centers are relative to the container holding the resource; material IDs 1, 2, and 3 have simple earth, green, and stone preview palettes.
3. Select `TerrainView` and click **Generate Preview**. Resource and container changes are explicit regeneration inputs; the prototype does not continuously rebuild on every Inspector change.
4. Change a child's position or area and regenerate to inspect scoped effects. Ancestor decorators run once across their scope; child scope does not duplicate ancestor operations. Resources are compiled into snapshots rather than modified for inheritance.
5. Toggle root **Bounded**. When true, generated geometry is clipped to the intersection of the root area and view preview area. When false, the root domain is unbounded, but this view still generates only its finite preview area. The demo requests a 10 m cube while the default root area is an 8 m cube, so toggling boundedness visibly changes the generated extent. The overlay displays the effective size. A zero-sized bounded area is invalid, not unbounded.

Smaller priority values run first. Equal values retain scene-tree preorder and decorator-list order. This slice has no random generator: fixed sphere parameters produce repeatable geometry without a pretend seed setting. Randomized layout and seed-derived operations remain future work.

## C++ reading order

| File | What to look for |
| --- | --- |
| `register_types.cpp` | ClassDB registration makes the four editor-facing types available. |
| `world_decorator.h/.cpp` | Resource properties and the abstract compile contract; spheres compile to immutable modifiers. |
| `region_container.h/.cpp` | Container properties, recursive scope collection, stable attachment ordering, and translation of modifier coordinates. |
| `terrain_data.h/.cpp` | Plain chunk/sample data, world-space flat base, fixed global sample lattice, narrow signed values, and uniform-channel compression. |
| `terrain_mesher.cpp` | Marching tetrahedra, border-compatible cube subdivision, normals, exact preview clipping, and the provisional material palette. |
| `terrain_view.h/.cpp` | Synchronous coordinator and Godot adapter: prepare a complete candidate, construct its meshes/static collision, and replace the old preview only after success. |

The prototype combines coordinator and view in `TerrainView` to keep the first slice small. Sampling and meshing remain separate from scene presentation. Registration, `_bind_methods()`, and `ADD_PROPERTY` expose C++ to the Inspector; `GDREGISTER_ABSTRACT_CLASS` exposes the decorator contract without allowing the base to be instantiated.

## Data, limits, and honest boundaries

Defaults are 10 cm spacing and 32 cells per chunk edge, with 33 samples per edge for meshing. Field values are 32-bit floats clamped to four sample spacings on either side of the surface; materials are 16-bit IDs. Uniform field/material chunks release their arrays. This samples candidate chunks before compression; it is not a production sparse hierarchy or an optimal generation pipeline.

Both preview sample count and generated triangle count are bounded. Failed validation/budget checks retain the previous published data and scene. `get_status()` explains failures and `get_statistics()` reports only the last successful result. `sample_field()` inspects that result's compiled analytic field, not a trilinear sample of its mesh or collision; it can be used to verify snapshots.

This slice supports translated, axis-aligned containers and an identity-transform view. Rotated/scaled containers are rejected. Child effects are hard-scoped to all ancestor boxes, without blended boundaries. Children have explicitly authored areas; repeated procedural layout selection is not implemented yet.

Deferred: streaming, multi-LOD meshes/stitches, saved edits, runtime digging/spells, antidecorators and cascade overrides, biome transitions, object cutting, and background execution. Synchronous editor scene replacement is not a claim of atomic production render/physics publication. Ten-centimetre spacing is experimental and does not guarantee every fist-sized feature in every grid alignment.

The first dedicated module test structure follows the reviewed initial implementation commit. Current verification uses a temporary smoke harness, the existing engine suite, and graphical editor/demo checks. The initial implementation commit follows user review. Remote pushes still require explicit authorization.
