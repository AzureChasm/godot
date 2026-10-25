# World Generator Terrain Prototype

This is the first reviewable C++ module prototype on `feature/world-generator-prototype`. It demonstrates the container/decorator authoring model and smooth three-dimensional terrain. The user reviewed the working prototype and its initial corrections and authorized this initial implementation commit.

## Build and open

From the Godot repository root:

```sh
scons platform=linuxbsd target=editor dev_build=yes tests=yes compiledb=yes -j16
./bin/godot.linuxbsd.editor.dev.x86_64 --editor --path modules/world_generator/demo
```

F5 opens `unbounded_world.tscn`. Open `terrain_gallery.tscn` or the original `demo.tscn` and use F6 to run those earlier demos.

The demo is a standalone Godot project inside this module; it has no dependency on Astray. Terrain generates automatically only when running the scene (F6) or project (F5), not when opening the scene in the editor. The Inspector action can still generate an explicit editor preview. Right-drag orbits, the mouse wheel zooms, R regenerates, and Space toggles root boundedness while keeping the same finite view request.

The initial scene contains a hill with a carved cavity, a floating solid in a translated child container, and another child-scoped mound. The overlay reports generation time, chunks, compressed uniform chunks, and triangles. These are prototype measurements, not production performance guarantees.

## Unbounded biome demo

F5 runs `unbounded_world.tscn`. Its unbounded `WorldRegionLayout` root uses **Voronoi** allocation. Child containers remain reusable biome definitions with occurrence weights and decorator resource lists. Zero weight excludes a biome; all-zero weights retain the previous preview with an error.

**Arrow keys continuously pan the camera**, relative to its viewing direction; Shift moves faster. Terrain stays at fixed world coordinates. Camera focus and loaded preview center are independent. The optional automatic refresh waits 0.3 s after movement pauses, then loads a window aligned to 16 m chunks. It never recenters the camera. Disable automatic refresh to inspect existing terrain freely and use **Generate / R** to load around the camera manually. Refreshes now run asynchronously on section workers; progress and Cancel remain usable while loading. Right-drag orbits; the wheel zooms. Click terrain to release focus after typing in a field so arrows control the camera again.

Parent distribution controls, applied with **Generate / R**:

| Control | Effect |
| --- | --- |
| Occurrence weights | Relative selection chances per region, independent of region area. |
| Distribution | Voronoi or the earlier grid for comparison. |
| Cell size | Typical fine Voronoi site spacing; demo 12 m. |
| Site jitter | 0 gives regular sites, 1 gives maximum bounded irregularity. |
| Avg cells/region | Coarse region density; demo 4. Set 1 for one region per fine cell. Actual counts vary. |
| X/Z stretch | Anisotropic parent allocation, for example elongated regions. |
| Layout seed | Repeatable sites, grouping and biome selection. |
| Transition | Proximity blend support; Voronoi caps it at half the cell size before stretch. |

**One region can be a union of several Voronoi cells.** Fine-cell sites are assigned to their nearest coarse region site. The owner selects a biome once and supplies a shared template origin across its cells. Their blend weights combine before evaluating the template, avoiding internal terrain seams. Group size is an approximate average, not an exact count or guaranteed connected shape. Neighboring owners can independently select the same biome. The 2D polygons are extruded vertically; vertical layer allocation remains deferred.

**V** toggles the allocation map: dark lines divide fine cells; white lines divide region owners. Map colors identify biomes. **G** toggles the 16 m chunk grid and region labels; **W** toggles the sea-level guide. These overlays distinguish storage chunks, Voronoi cells, and biome regions. Runtime edits are temporary; save persistent settings on root/children in the Inspector. Camera speed/follow controls apply immediately.

Parent allocation supersedes immediate child template boxes/placement. Descendant enclosures remain relative to the instance; nested layouters compile recursively. Logical region instances are sampled on demand rather than spawning an infinite scene tree. Child programs execute internally in priority order as one layout operation at -100; later parent decorators can carve the blended result.

**Transitions** blend signed fields of neighboring regions with normalized compact proximity weights, including multi-cell junctions. Internal fine-cell borders do not create separate terrain patterns. At zero width, boundaries are hard; increasing width broadens the blend. Voronoi band thickness depends on site geometry and allocation stretch. Material IDs still choose the strongest region; specialized beaches/cliffs and material blending are later work.

The default loaded window is 64 × 32 × 64 m at coarse 0.5 m spacing, with static collision. **View radius** adjusts the horizontal half-extent from 16 to 128 m; height remains 32 m. **Samples (m)** changes resolution, **Workers** selects 1–16 tasks, and **Collision** can be disabled for rendering-only experiments. Apply with Generate/R; **Frame view** explicitly adjusts camera distance to fit the selected radius. Camera position is never reset by generation. The menu scrolls on smaller windows. This remains a finite moving preview, with the prototype's 100000 m coordinate guard and sample/triangle limits; it rebuilds whole windows rather than incrementally streaming/cache-reusing sections. Repeated region instances reuse their authored template patterns. The top-down diagnostic map adds UI work beyond the native generation timings.

## Fixed world chunks and performance

A **world chunk is always 16 × 16 × 16 m**, independent of resolution. Internal **sections** divide sampling and meshing work. `TerrainView.section_cells` is a preferred section cell count (default 32); a suitable lattice divisor is chosen at generation. Requested spacing is snapped to fit whole sections into 16 m chunks, never made coarser. Statistics report actual spacing, section size/count and world chunk count separately. World chunks are spatial address groups; arrays and meshes live in their sections. The chunk count reports groups overlapped by the request, including air/solid regions. The legacy serialized `chunk_cells` setting remains loadable.

Surface-only programs reuse one noise evaluation per XZ column through the relevant vertical interval; arbitrary 3D modifiers automatically take the general path. Meshing reads shading gradients/materials from sampled sections with one-sample halos instead of repeatedly evaluating analytic noise per triangle vertex. Shared vertices use indexed meshes. The sampled tetrahedron gradient still determines topology/winding. Shading now approximates the sampled field rather than the analytic field.

A three-run, headless development-build comparison on this machine reduced the unchanged gallery's median generation from **3069.8 ms to 1388.9 ms** (about **2.2× faster**), still **135632 triangles with collision enabled**. The count changed from 48 old variable-size chunks to 8 fixed world chunks / 32 sections; that is a unit change, not deleting terrain. Sample payload grew from 10.35 MB to 15.27 MB due to 16 m alignment and gradient halos. These are local prototype measurements, not production budgets. Stage timings show mesh extraction and collision/publication remain significant. That benchmark predates threaded generation. Parallel section jobs now exist; incremental section reuse and mesh simplification remain future optimization work.

## Threading and responsiveness

`generate_preview()` now returns **request accepted**, not generation finished. Listen to `preview_generated`, `generation_failed` and `generation_cancelled`, or poll `is_generating()` while yielding frames:

```gdscript
if terrain.generate_preview():
    while terrain.is_generating():
        await get_tree().process_frame
    print(terrain.status) # Inspect errors; statistics retain the last success.
```

Only configuration capture reads nodes/resources on the main thread. The dedicated coordinator plans the cache and dispatches Godot worker-pool tasks. Each task owns one preallocated section/result slot; shared modifiers and input settings are immutable. Sampling, meshing, vertex remapping, and collision-face expansion happen off the main thread. Cancellation is checked between sample/mesh rows and publication pieces. A new request cancels obsolete work and keeps only the latest pending snapshot. Closing/freeing the scene cancels and joins its workers.

Godot scene/render/physics registration stays on the main thread. Workers split geometry into at most 1024-triangle pieces, registered under a default **4 ms soft frame budget**. Staging meshes are hidden and collision-disabled until complete; then the old view is hidden/disabled and the new one is activated. Old scene nodes retire incrementally. A single backend call, final swap, GPU/physics synchronization or allocation can exceed that soft budget; there is no hard frame-time guarantee. More publication pieces also increase draw-call/node counts compared with one mesh per section. Dedicated render batching remains future work.

The overlay separates total elapsed time, worker wall time and accumulated scene/physics work. Detailed statistics include worker count, a main-thread-generation diagnostic, CPU sums, largest publication batch/commit, and sample payload. **Summed section timings are neither worker wall time nor OS CPU-time counters**; they include scheduling/preemption. The diagnostic map also yields between small UI work batches.

View-distance experiments default to **16 million samples / 2 million triangles**, with configurable Inspector budgets capped at 64 million samples / 4 million triangles and a 4096-section ceiling. Guards may stop a test before a hardware limit; failures retain the previous terrain. Increasing radius at fixed spacing grows horizontal sampling/meshing roughly with area; sample payload excludes meshes, collision and overlap between previous/staging views. Use coarser samples for larger windows rather than silently changing authoritative resolution. Unsupported non-threaded builds fail explicitly instead of falling back to main-thread generation.

Targeted checks verified 8 actual workers with no main-thread generation, unchanged 214973-triangle default Voronoi output, cancellation/supersession, safe oversized requests, collision raycasts and freeing during active work. Menu checks completed 64 m and 128 m radius previews at 1 m spacing; the latter covered 512 world chunks with collision disabled. These are local development-build observations, not production limits or performance guarantees.

## Terrain gallery

Five resources share elevation, amplitude (relief), wavelength, octaves, seed, material and blend controls:

| Decorator | Default elevation / relief / wavelength (metres) | Character |
| --- | --- | --- |
| `WorldPlainsDecorator` | 0.1 / 0.08 / 5 | Mostly even |
| `WorldHillsDecorator` | 0.65 / 1.1 / 4.5 | Smooth rolling waves |
| `WorldMountainsDecorator` | 0.3 / 3.3 / 8 | Ridged mountain masses |
| `WorldSeaDecorator` | -1.5 / 0.25 / 5 | Shallow sandy seabed |
| `WorldDeepSeaDecorator` | -3.5 / 0.7 / 3.5 | Deeper, rougher seabed |

Press **0** for all tiles or **1–5** to focus a type. Select a profile, adjust the runtime fields, then press **Generate/R**. **W** toggles the Y=0 water-level guide; **C** toggles a mountain cave. Runtime adjustments are temporary; edit each container's decorator resource in the Inspector to save settings. Right-drag orbits and the wheel zooms.

Profiles run at priority -100 before spheres (default 0). Replace sets a new field, Add Solid preserves previous solid, and Remove Solid preserves previous air. These topographic operations still write the same 3D field; the cave demonstrates later 3D carving. Noise reuses Godot's bundled FastNoiseLite implementation without depending on its optional noise module.

The gallery uses 20 cm samples for a manageable bounded preview; the module default remains 10 cm. Tile edges are deliberately hard for comparison; biome transitions remain deferred. Sea types generate seabeds; the translucent guide is not simulated water.

## Edit the prototype

1. Select `World`. Only containers are children of this node. Its **Decorators** property is a list of `WorldDecorator` resources.
2. Expand the sphere resources and change their center, radius, operation, material ID, or priority. Centers are relative to the container holding the resource; material IDs 1 through 5 have earth, green, stone, sand, and deep-slate preview palettes.
3. Select `TerrainView` and click **Generate Preview**. Resource and container changes are explicit regeneration inputs; the prototype does not continuously rebuild on every Inspector change.
4. Change a child's position or area and regenerate to inspect scoped effects. Ancestor decorators run once across their scope; child scope does not duplicate ancestor operations. Resources are compiled into snapshots rather than modified for inheritance.
5. Toggle root **Bounded**. When true, generated geometry is clipped to the intersection of the root area and view preview area. When false, the root domain is unbounded, but this view still generates only its finite preview area. The demo requests a 10 m cube while the default root area is an 8 m cube, so toggling boundedness visibly changes the generated extent. The overlay displays the effective size. A zero-sized bounded area is invalid, not unbounded.

Smaller priority values run first. Equal values retain scene-tree preorder and decorator-list order. Sphere parameters produce repeatable geometry; the new surface profiles also expose deterministic noise seeds. Procedural child layout remains future work.

## C++ reading order

| File | What to look for |
| --- | --- |
| `register_types.cpp` | ClassDB registration makes the eleven editor-facing types available. |
| `world_decorator.h/.cpp` | Resource properties and the abstract compile contract; spheres compile to immutable modifiers. |
| `world_region_layout.h/.cpp` | Parent allocation, weighted child definitions, deterministic cell hashing and field transitions. |
| `world_terrain_decorator.h/.cpp` | Shared profile properties, preset subclasses, deterministic noise and signed-field composition. |
| `region_container.h/.cpp` | Container properties, recursive scope collection, stable attachment ordering, and translation of modifier coordinates. |
| `terrain_data.h/.cpp` | Plain chunk/sample data, world-space flat base, fixed global sample lattice, narrow signed values, and uniform-channel compression. |
| `terrain_mesher.cpp` | Marching tetrahedra, border-compatible cube subdivision, normals, exact preview clipping, and the provisional material palette. |
| `terrain_build_job.h/.cpp` | Dedicated coordinator, parallel independent section outputs, cooperative cancellation, collision-face preparation and bounded publication pieces. |
| `terrain_view.h/.cpp` | Main-thread adapter: capture authoring values, poll worker completion, register small hidden scene pieces across frames, then swap the complete preview. |

The worker coordinator is now separated into `TerrainBuildJob`; `TerrainView` owns its lifetime and main-thread presentation. Sampling and meshing remain separate from scene presentation. Registration, `_bind_methods()`, and `ADD_PROPERTY` expose C++ to the Inspector; `GDREGISTER_ABSTRACT_CLASS` exposes the decorator contract without allowing the base to be instantiated.

## Data, limits, and honest boundaries

Defaults are requested 10 cm spacing, fixed 16 m world chunks and preferred 32-cell internal sections. Sections store cells + 3 samples per edge for shared borders and shading-gradient halos. Field values are 32-bit floats clamped to four sample spacings on either side of the surface; materials are 16-bit IDs. Uniform field/material chunks release their arrays. This samples candidate sections before compression; it is not a production sparse hierarchy or an optimal generation pipeline.

Both preview sample count and generated triangle count are bounded. Failed validation/budget checks retain the previous published data and scene. `get_status()` explains failures and `get_statistics()` reports only the last successful result. `sample_field()` inspects that result's compiled analytic field, not a trilinear sample of its mesh or collision; it can be used to verify snapshots.

This slice supports translated, axis-aligned containers and an identity-transform view. Rotated/scaled containers are rejected. Manually positioned child effects are hard-scoped to ancestor boxes. `WorldRegionLayout` instead assigns cells and blends their fields; it does not yet implement organic region allocation.

Deferred: streaming, multi-LOD meshes/stitches, saved edits, runtime digging/spells, antidecorators and cascade overrides, specialized biome transitions, and object cutting. Staged preview replacement is not a claim of atomic production physics-tick publication. Ten-centimetre spacing is experimental and does not guarantee every fist-sized feature in every grid alignment.

The user has deferred a dedicated unit-test suite at this stage; the demo and user inspection lead validation. Current verification uses a temporary smoke harness, the existing engine suite, and graphical editor/demo checks. The initial implementation commit follows user review. Remote pushes still require explicit authorization.
