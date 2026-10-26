# World Generator Terrain Prototype

This is the first reviewable C++ module prototype on `feature/world-generator-prototype`. It demonstrates the container/decorator authoring model and smooth three-dimensional terrain. The user reviewed the working prototype and its initial corrections and authorized this initial implementation commit.

Current iterations are on `feature/world-generator-elevation-lod`; the original prototype milestone above is historical. The latest scene includes incremental streaming, while older scenes retain their whole-window preview behavior.

## Build and open

From the Godot repository root:

```sh
scons platform=linuxbsd target=editor dev_build=yes tests=yes compiledb=yes -j16
./bin/godot.linuxbsd.editor.dev.x86_64 --editor --path modules/world_generator/demo
```

F5 opens the latest cumulative scene, `010_climate_coasts_and_drainage.tscn`.
Open an earlier numbered scene and press F6 to review that historical milestone.

| Scene | Milestone |
| --- | --- |
| `001_terrain_primitives.tscn` | Original bounded field, nested scopes, solid addition and carving |
| `002_terrain_gallery.tscn` | Five editable terrain profiles and cave comparison |
| `003_voronoi_world.tscn` | Weighted unbounded regions, grid/grouped Voronoi, camera, workers and LOD controls |
| `004_nested_regions_and_ravines.tscn` | Cumulative world with nested allocation and optional varied cuts at four scales |
| `005_landforms_and_ridges.tscn` | Structural mountain ridges, nine new landform types and layered terrain composition |
| `006_heightmap_evaluated_terrain.tscn` | Heightmap-guided broader, unequal summits and less regular ridges |
| `007_varied_regions_and_scale.tscn` | Irregular region edges, varied terrain families, budget-fitting distant views and a1.8m capsule |
| `008_streaming_biomes_and_rivers.tscn` | Prioritized incremental residency, fourteen terrain biomes, river courses and larger alpine ridges |
| `009_lakes_plateaus_and_terrain_variety.tscn` | Elevated lakes, river grades, twenty layered biome types, plateaus and near-field simplification |
| `010_climate_coasts_and_drainage.tscn` | Climate hierarchy, larger oceans/trenches, terrain-routed rivers, coastal/highland lakes and independent water visibility |

Use `NNN_descriptive_name.tscn` and a matching `.gd`/`.gd.uid` for each new milestone.
Preserve script UIDs on rename. Every newest demo must carry forward existing
features and controls so interactions can be reviewed together; earlier scenes
remain snapshots. Shared UI/camera glue lives in the 003 script and is extended
by 004; heavy generation stays in C++. Update F5's main scene when adding a milestone.

The demo is a standalone Godot project inside this module; it has no dependency on Astray. Terrain generates automatically only when running the scene (F6) or project (F5), not when opening the scene in the editor. The Inspector action can still generate an explicit editor preview. Right-drag orbits, the mouse wheel zooms, R regenerates, and Space toggles root boundedness while keeping the same finite view request.

The initial scene contains a hill with a carved cavity, a floating solid in a translated child container, and another child-scoped mound. The overlay reports generation time, chunks, compressed uniform chunks, and triangles. These are prototype measurements, not production performance guarantees.

## Unbounded biome demo

Open `003_voronoi_world.tscn` with F6 for the earlier unbounded milestone. Its unbounded `WorldRegionLayout` root uses **Voronoi** allocation. Child containers remain reusable biome definitions with occurrence weights and decorator resource lists. Zero weight excludes a biome; all-zero weights retain the previous preview with an error.

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

The default loaded window is 64 × 32 × 64 m at coarse 0.5 m spacing, with static collision. **View radius** adjusts the horizontal half-extent from 16 to 256 m; height remains 32 m. **Samples (m)** changes resolution, **Workers** defaults to the OS logical processor count and allows an explicit positive task limit, and **Collision** can be disabled for rendering-only experiments. Apply with Generate/R; **Frame view** explicitly adjusts camera distance to fit the selected radius. Camera position is never reset by generation. The menu scrolls on smaller windows. This remains a finite moving preview, with the prototype's 100000 m coordinate guard and sample/triangle limits; it rebuilds whole windows rather than incrementally streaming/cache-reusing sections. Legacy profiles use template-local patterns; the cumulative demo opts into continuous world-space noise. The top-down diagnostic map adds UI work beyond the native generation timings.

## Fixed world chunks and performance

A **world chunk is always 16 × 16 × 16 m**, independent of resolution. Internal **sections** divide sampling and meshing work. `TerrainView.section_cells` is a preferred section cell count (default 32); a suitable lattice divisor is chosen at generation. Requested spacing is snapped to fit whole sections into 16 m chunks, never made coarser. Statistics report actual spacing, section size/count and world chunk count separately. World chunks are spatial address groups; arrays and meshes live in their sections. The chunk count reports groups overlapped by the request, including air/solid regions. The legacy serialized `chunk_cells` setting remains loadable.

Surface-only programs reuse one noise evaluation per XZ column through the relevant vertical interval; arbitrary 3D modifiers automatically take the general path. Meshing reads shading gradients/materials from sampled sections with one-sample halos instead of repeatedly evaluating analytic noise per triangle vertex. Shared vertices use indexed meshes. The sampled tetrahedron gradient still determines topology/winding. Shading now approximates the sampled field rather than the analytic field.

A three-run, headless development-build comparison on this machine reduced the unchanged gallery's median generation from **3069.8 ms to 1388.9 ms** (about **2.2× faster**), still **135632 triangles with collision enabled**. The count changed from 48 old variable-size chunks to 8 fixed world chunks / 32 sections; that is a unit change, not deleting terrain. Sample payload grew from 10.35 MB to 15.27 MB due to 16 m alignment and gradient halos. These are local prototype measurements, not production budgets. Stage timings show mesh extraction and collision/publication remain significant. That benchmark predates threaded generation. Parallel section jobs and initial mesh simplification now exist; incremental section reuse remains future optimization work.

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

View-distance experiments default to **16 million samples / 2 million triangles**, with configurable Inspector budgets capped at 64 million samples / 4 million triangles and a 16384-section ceiling. Guards may stop a test before a hardware limit; failures retain the previous terrain. Increasing radius at fixed spacing grows horizontal sampling/meshing roughly with area; sample payload excludes meshes, collision and overlap between previous/staging views. Use coarser samples for larger windows with the effective preview resolution displayed explicitly. Unsupported non-threaded builds fail explicitly instead of falling back to main-thread generation.

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


## Nested surface and first distance LOD

The earlier `demo/004_nested_regions_and_ravines.tscn` remains available with F6. The newest F5 demo, 005, carries its controls forward. It combines all five terrain
profiles, independent camera motion, weighted grid/grouped Voronoi allocation,
transitions, elevation, strata, cave cuts/grounded solid additions, threaded previews, collision,
view distance, cancellation, maps/grids and distance LOD.

The authored tree is `World → biome → Gullies → Fissures → Channels`.
Each biome is itself a layouter, with three distinct terrain formations plus a further subdivided cut branch.
The twenty editable profiles include grass flats, rolling meadows, rocky
outcrops/ridges, sheltered valleys, mountain flanks/sharp peaks/high plateaus,
sandy shelves, seabed ridges/basins, abyssal plains and submarine ridges. Cuts can occur at levels 1–4: reference lengths are 40, 21, 12 and 7 m,
with independently tuned widths and depths. They are metre settings, not implicit
scene-node scaling. Larger-parent cuts run after their nested child program.
A region can have no cut, a large cut, several small cuts, or a combination.

The L1–L4 toggles isolate each scale. **Cut chance ×** adjusts occurrence,
**Cut variation** changes seeded orientation/position/dimensions/bends, and
**Child cell scale** adjusts nested allocation sizes (minimum cell size 4 m).
Expand **Edit terrain profiles** to select any of the twenty terrain profiles and change
local elevation, relief, wavelength, seed, octaves, material ID, rocky detail,
peak sharpness, interior elevation/relief/ramp width and continuous world noise.
**Elevation ×**, **Strata**, **Cave**, **Rock outcrops** and **Bound world** make
interactions reviewable. The optional bound is a fixed 256 × 128 × 256 m box.
**Map level** selects the nested allocation shown by labels and the map; branches
can end earlier in an undisturbed leaf. Map colours retain their top-level biome.
All changes apply with Generate/R; old terrain remains until publication succeeds.
The initial request is 256 × 128 × 256 m at 1 m spacing; reduce spacing to inspect
small channels and use the retained view-distance/LOD controls to balance cost.

Demo cut probabilities are now 4% for large cuts and 8% for smaller cuts, before
weighted subregion selection. The multiplier can increase these for stress tests.
The former floating add-solid sphere has been removed; solid additions are
grounded height-field rock outcrops. Moving/floating islands remain deferred.

Ravine probability and variation use a stable instance identity passed down the
layout tree. Identical inputs regenerate identical cuts independent of workers or
exploration order, but different instances need not repeat one template shape.
Noise terrain settings retain their own seeds. Default probability 1 and variation
0 preserve the original explicit ravine behaviour for existing resources.

Base elevation is inherited authoring metadata, resolved before compilation:
children can replace it or add a relative offset. Profiles and ravines use it;
container bounds and explicitly positioned spheres do not move. Add a terrain
profile even for flat ground: metadata alone does not replace the initial Y=0 field.
Nested scopes intersect, including inside instantiated layout templates.
Hard bounds can truncate a cut; keep its footprint inside the enclosure for a natural ending.
Strata are a two-material field-depth approximation, not underground volume allocation.

LOD currently simplifies each completed section mesh on workers, using Godot's
meshoptimizer callback with fixed topological borders. Full detail is retained
within `lod_distance` of `lod_origin`; beyond it and twice it the targets are
25% and 6.25% of source triangles, limited by error and border constraints.
`lod_error` is an absolute geometric-error metric (second level permits twice it).
A topology check rejects candidates that change open/nonmanifold edges, retaining
the original section mesh. No vertex positions move; shared section edges stay fixed. Normal/material
variation inside a removed patch is approximate. Collisions follow the reduced
mesh, so use full detail near gameplay interactions. Without meshoptimizer,
enabling LOD fails explicitly and retains the previous preview.

The LOD origin is supplied by the caller (the demos use camera focus), captured
per request. Moving it requires regeneration. Fixed 16 m chunks and full-resolution
sampling remain unchanged: this reduces submitted geometry, collision faces and
publication work, not voxel sampling or base meshing cost. This is not adaptive
sampling, automatic per-frame mesh switching or incremental streaming.
Statistics expose `source_triangles`, `triangles`, `lod_sections` and `lod_origin`.


## Continuous biome terrain and current performance defaults

The cumulative demo defaults to a 128 m view radius (256 m across), 128 m vertical
coverage, 1 m samples, 32 m full-detail radius and 0.25 m LOD error. The larger
request uses a 4-million output-triangle budget; normal sample/section limits
remain. Choosing finer samples or much larger radius can exceed them and retain
the last successful view with a clear error.

Worker count defaults to OS logical processors, without the old 16-task ceiling.
Tasks use the full worker queue, so a low-priority thread ratio does not silently
limit terrain parallelism. Explicit project pool settings and workload size still
cap actual use; `worker_limit` and `worker_threads_used` distinguish them.
Using every logical processor is the requested default, not a claim that every
workload scales linearly with thread count.

Root `biome_edges` supplies a capped local interior-depth estimate to descendant
profiles. It compares matching versus foreign biome sites, ignoring internal
same-type owner edges. World-space noise avoids resetting the profile at each
owner origin. Mountain outer terrain starts low; increased interior elevation,
relief, ridge sharpness and octave roughness create taller, rockier interiors.
The top-level map draws white borders only between different biome types; dark
lines still expose cells. Allocation owner IDs and seeded subregion instances
remain distinct: this is a continuous biome envelope, not global connected-region
indexing or an exact centroid/distance solution for potentially unbounded regions.

Profiles and boundary estimates use bounded thread-local XZ caches, keyed by
unique immutable compilation IDs and all relevant inputs. They reuse column work
even when caves, strata or ravines require general 3D sampling. No shared mutable
RNG/cache or scene/server access is introduced, and new requests cannot reuse
stale compiled values. These caches do not replace chunk streaming or LOD storage.


## Structural landforms and visual iteration (005)

Open `005_landforms_and_ridges.tscn` with F6 for that milestone; 008 is now the F5 cumulative scene. It retains the cumulative world,
nesting, terrain mixtures, rare ravines/caves, maps, grounded outcrops, all-core
workers, larger view and LOD controls. 004 is preserved for comparison.

The new native decorators are MountainRange, Foothills, Plateau, Mesa, Dunes,
Basin, Terraces, Valley and Escarpment (all `World...Decorator` Resources).
They implement different shapes, not just different noise amplitudes. Existing
containers remain sufficient: the new scene composes the forms in its region tree.

The mountain fallback now owns a broad graph of seeded summit points and their
connecting ridges/saddles. There are three summits per 96 m graph tile by default.
Its 3x3 neighbourhood uses shared endpoints and compact support, keeping evaluation
local and independent of chunks/discovery. The biome envelope clips/tapers this
world-space graph; points are not yet fitted to an exact connected biome polygon.
Small detail uses a 4% strength for restrained flank grooves and noise. This replaces the repeated
high-frequency ridged-noise summit pattern in the newest scene.

Mountain child regions apply **ADD_RELIEF** layers: shallow scree/foothill detail,
saddle valleys and benches. They cannot replace the range with another complete
spiky mountain profile. The incoming material is retained. Place such layers
before carving: shifting the whole signed field after a cave would shift it too.
Plains/hills gain mesas and plateaus; the shore gains dunes/shelves; seabeds gain
basins and broad submarine ranges. There are 23 authored profiles in the scene.

The assistant owns terrain method selection, presets, tuning, and the world tree
within the agreed architecture. User review can stay qualitative: silhouettes,
scale, slopes, valleys, transitions, and what feels repetitive or unnatural.
Reference photos help describe the desired appearance; they are optional and do
not become a request for the user to identify implementation parameters.
The advanced landform panel remains available for exploration and C++ review.

These are analytic procedural forms. No erosion, hydrology, geology simulation,
floating islands, persistence or 3D biome allocator is claimed by this iteration.


### Mountain reference direction

The user-provided images under `references/moutains/` are visual references, not
generated assets. Their original filenames are retained. `mountains1.jpg` and
`mountains2.jpg` show substantial connected rock masses, distinct major summits,
saddles and broad valleys. `mountains_with_hills_in_front.jpg` adds an important
scale transition from gentler hills into steep mountain faces. The render in
`3d-render_mointain.jpg` emphasizes branching ridge structure and grooves running
down slopes. These guide geometry and composition; snow, forests, clouds and
photoreal materials are not implemented by this terrain iteration.

005 adds restrained radial flank grooves, rounded valley floors, and less intense
preview lighting to make those shapes easier to assess. Reference photos guide
qualitative review; matching their geology exactly is not claimed.


## Heightmap diagnostics and comparison (006)

006 retains the full cumulative tree/controls and adds broad rounded summits,
dominant/secondary height bands and less regular ridge connections. The earlier
005 remains the baseline. Heights remain a diagnostic projection of the 3D field;
the world representation, caves, chunks and mesh LOD are unchanged.

Set `TerrainView.heightmap_csv_path` before generating to opt into native export.
The coordinator reads the completed cache off the main thread, scans from above
for the first air-to-solid crossing, interpolates its height, and atomically
renames the finished CSV. `heightmap_stride` controls sampling of the existing
lattice; it does not regenerate a coarser field. Clipped-top and missing-surface
columns remain invalid, never fake flat peaks. Normal generation does no export.
Check `statistics.heightmap_error` and the CSV/request IDs; diagnostics describe
a completed candidate and are not part of the later visibility/collision swap.

From the engine root, export a paired comparison with the same layout seed:

```sh
./bin/godot.linuxbsd.editor.dev.x86_64 --headless \
  --path modules/world_generator/demo --script res://tools/export_heightmap.gd \
  -- --scene res://005_landforms_and_ridges.tscn --seed 2026 \
  --output /tmp/worldgen-heightmap/before.csv
./bin/godot.linuxbsd.editor.dev.x86_64 --headless \
  --path modules/world_generator/demo --script res://tools/export_heightmap.gd \
  -- --scene res://006_heightmap_evaluated_terrain.tscn --seed 2026 \
  --output /tmp/worldgen-heightmap/after.csv
python3 modules/world_generator/demo/tools/analyze_heightmap.py \
  /tmp/worldgen-heightmap/after.csv --baseline /tmp/worldgen-heightmap/before.csv \
  --output /tmp/worldgen-heightmap/report
```

The optional Python report tool uses NumPy/Matplotlib; the native engine has no
Python dependency. It produces raw height/contour maps, slope maps, high-pass
residuals, peak CSVs, metrics JSON, and paired PNG/Markdown comparisons. It checks
matching coordinates and, when export manifests exist, matching layout seeds.
Source CSV hashes, scene hashes, analysis-tool hash, sampling and thresholds are
recorded for review. Standalone exports default to the project's latest scene;
`--stride`, `--seed` and `--workers` are optional batch overrides.

The report uses descending 8-connected components to estimate finite-window
peak prominence. Defaults select prominence >=2m, omit an8m boundary margin and
label high terrain at elevation >=8m (a height proxy, not semantic biome IDs).
A narrow-tip flag means at least1m mean drop on a2m ring around a selected peak.
Censoring and coarse sampling are flagged. These are interpretable heuristics,
not a universal art score. Hidden overhangs/caves and final mesh/lighting artifacts
are absent from this top-surface projection; inspect the actual render as well.

Evaluate the composed scene, not just an isolated decorator or raw maximum count.
Compare sharpness together with retained relief and peak-height variation: a flat
landscape would otherwise look deceptively successful. Keep before/after seed,
footprint, spacing and thresholds fixed, and check more than one seed. A low,
uniform intermediate revision was rejected using those checks during this pass.

### 007: region variety, irregular boundaries and scale

`007_varied_regions_and_scale.tscn` is the cumulative F5 demo. Mountain
rounding is reduced while modest flank relief restores rocky detail. Plains
retain flatter patches among rolling meadows and terraces; hills gain broader
relief, coasts stronger shelves/dunes, and deep water stronger basins/ridges.
These are authored compositions of the existing native decorators, not erosion.

`WorldRegionLayout.boundary_noise` (0–0.4 cell widths, default zero) smoothly
warps Voronoi query coordinates at two scales. Ownership, blends, biome-depth
estimates and diagnostic maps use the same transformation. Grid mode is unchanged.
Root and nested layouts in007 use different amounts; the root has a UI control.

“Fit sampling to preview budget” defaults on in007. It keeps the chosen radius
and, when needed, selects a coarser whole-window lattice (up to2m). Requested and
effective spacing are displayed. This compensates for full-resolution sampling
preceding meshLOD; it is not distance-adaptive sampling or incremental streaming.
The native section ceiling is16384; the independent sample and triangle budgets
remain enforced. Disabling the option retains fixed-resolution failure behavior.

An orange capsule is1.8m tall and0.6m wide. Its feet follow the highest terrain
crossing at X/Z=0 after regeneration, independently of collision settings. It is
a stationary scale marker, not a playable character; its label remains legible
from the overview; use “View1.8m scale capsule” for a close-up. It hides when the origin is outside the loaded window.

## 008: movement-prioritized streaming, biomes and rivers

F5 now opens `008_streaming_biomes_and_rivers.tscn`. The view-radius control and
`TerrainStreamer.view_distance` allow **32–2048 m** in streaming mode. Larger
radii queue additional terrain progressively; they do not allocate the entire
source volume or bypass memory/triangle limits. The default radius is256 m.
The separate whole-window preview remains available by disabling “Stream terrain”
and pressing Generate; its allocation guards still apply.

`TerrainStreamer` keeps resident **64 m XZ patches**, each containing fixed16 m
world chunks throughout the configured vertical enclosure. It freezes the
compiled world program and generation settings until `rebuild()`/Generate.
Movement calls `set_focus(position, direction)` and keeps overlapping patches;
nearby work takes priority, with preference ahead of movement. Obsolete work is
cancelled, outgoing terrain retires in stages, and approaching patches refresh
mesh LOD with hysteresis. Geometry remains at fixed world coordinates. Root area
constraints and all nested decorator scopes still apply.

The coordinator and sampling/meshing use workers. Terrain pool tasks now use
low priority, allowing the engine's configured low-priority thread ratio to
reserve capacity for other tasks. The requested worker count still defaults to
logical CPUs; actual concurrency may be lower. Mesh/physics publication has a
**soft 1 ms/frame budget per active patch**, with a separately staged retirement
queue. An individual server call can exceed a soft budget; this is not a hard
frame-time guarantee. Resident meshes share the template's triangle cap, favouring
nearer patches when capacity is tight. Source arrays are released after each
patch publishes; immutable programs remain available for diagnostics.

This first streamer uses a uniform source lattice for seamless borders, plus
existing mesh LOD. It does not implement adaptive-distance source sampling,
persistence/player edits, simulation, or vertically moving residency bounds.
The008 vertical enclosure is−64..192 m. The bounded per-worker layout-plan cache
avoids recomputing ownership/weights for every vertical sample and copies a plan
before recursion so child cache collisions cannot change its parent.

The cumulative tree now has **14 weighted terrain biome definitions**: plains,
hills, mountains, sea, deep sea, desert, canyons, badlands, savanna, wetlands,
tundra, forest, taiga and rainforest. Each new type composes existing reusable
landform decorators; forest names specify terrain, without claiming vegetation.
The mountain preset uses140 m relief,160 m wavelength, broad connected ridges and
less rounded summits. It retains varying dominant/secondary summit heights.
All profiles, region weights, noise, nested cuts, scale capsule and earlier
review controls remain available. Finite windows need not contain every biome.

`WorldRiverDecorator` provides an editable downhill `centerline`, channel width,
bank width, depth, meander and seed. It only carves existing terrain, so it cannot
fill/dam another cut. Invalid uphill paths fail compilation. `create_water_mesh`
returns a matching render ribbon;008 loads its segments with resident patches.
Two root decorators demonstrate a main course and joining tributary. This is
an authored procedural course, not automatic drainage, erosion, flowing water
or a guarantee that an arbitrary edited path is physically valid everywhere.

Heightmap export automatically disables streaming for a complete repeatable
window. Use `tools/export_heightmap.gd -- --scene res://008_streaming_biomes_and_rivers.tscn --seed 2026 --output /tmp/terrain.csv`
with the same engine/script command shown in006. Keep seed, window and lattice
fixed when comparing. Added biomes change the allocation itself; report those
changes rather than attributing every metric difference solely to mountains.

## 009: elevated lakes, river grades and layered landforms

The latest cumulative scene is `009_lakes_plateaus_and_terrain_variety.tscn`.
Camera speed is an immediate **2–600 m/s slider**, default 72 m/s, near the top of
the menu. Arrow movement uses that value directly. G creates/toggles the streamed
grid/region-label overlay; labels rebuild as residency changes, skip unloaded
positions, and use the actual highest surface crossing, including negative Y.
Label creation yields between columns to keep debug work distributed over frames.
The16 m grid lines remain a sea-level reference.

There are now **20 terrain biome definitions and 109 authored formations**.
Highlands, alpine meadows, steppe, mangroves, volcanic terrain and karst join008's
fourteen types. Direct formation leaves include extra relief and contour layers:
332 editable terrain-profile resources in total, plus strata and scoped cuts.
Selector entries distinguish base profiles from layers. Root biome elevation
datums create broad lowland/upland differences; tablelands, scarps, terraces and
valleys add stronger plateau forms. These compose the existing native landforms,
not vegetation, erosion or an automatic climate model. Earlier trees remain in
older scenes for comparison.

`WorldRiverDecorator.slope` adds downstream fall in metres per horizontal metre
on top of authored point heights. Its generated centerline, carved bed and water
ribbon share this grade. The009 demo has a grade control, preserves its tributary
junction elevation, and matches the connected lake's water level on Generate.
River ribbon vertices within a standing lake conform to the lake surface. This
is static water geometry rather than a flowing-water simulation.

`WorldLakeDecorator` builds a complete grounded basin and enclosing banks,
including any necessary local filling, then later cave/river operations can cut
it. `center.y` is its independent standing-water elevation. Radius, depth, outer
bank width, shore height/noise and seed are configurable. Water disks use the
same shoreline and clip exactly to loaded patch bounds. The demo includes a
river lake near 6 m and a separate upland lake at 72 m. These are root-authored
examples; rendering water for generated nested instances still requires the
caller to apply instance transforms/scopes.

`TerrainView.lod_near_error` optionally simplifies near meshes without changing
source samples (default 0 preserves earlier behavior).009 uses 0.08 m and targets a
quarter of near source indices. Fixed boundary vertices and topology checks
remain mandatory; actual reduction depends on geometry. `lod_max_level` permits
1–4 bands, default 2;009 uses 3. Distance bands double their threshold and geometric
error per band.009 uses 24 m first threshold and 0.5 m base distant error. Collision
follows the reduced mesh; the full signed field remains the diagnostic source.
The default overview uses **2 m source samples** for faster loading; select 1 m or finer with the existing sample control to inspect small cuts. This changes preview sampling, not the continuous world field. Batch exports accept `--spacing 1` for fixed-resolution comparisons.

The existing source-memory guards, priority streaming and resident triangle cap
remain in force.

Boundary repair can retain a few more triangles at a coarser band; band targets
are not a promise of strictly decreasing counts. A 2 m test window retained 37577
triangles versus 37551 at the preceding maximum band, both below half the 124965
source triangles, with identical boundary topology. Near-field reduction and
coarser source sampling provide the main gains in this iteration.

## Climate, coasts and drainage (010)

The newest tree separates broad **Land/Oceans**, then dry/humid/temperate/cold/upland families, then biomes and existing nested formations. All earlier terrain/decorator controls remain; expand **Climate / biome weights** to change nested occurrence weights. These weights choose occurrences rather than guaranteeing area percentages. Oceans use larger root regions; Sea, DeepSea and Trenches deepen toward marine interiors. The vertical study enclosure is now -288..224m.

`WorldRiverRouter` captures the pre-river scalar field and plans on workers over a fixed 2560m square around the origin. A noisy Voronoi drainage graph connects highland sources to boundary-connected ocean outlets, with descending water heights and bounded incision. Rejected paths are omitted rather than forced through ridges. Camera movement retains the same courses. This prototype does not yet generate infinite catchments, rainfall or erosion. See [research notes](references/terrain-generation.md).

Coastal lake candidates target Y=0; inland highland lakes derive water level from nearby terrain. Placement avoids accepted river courses. **Sea surface**, **Lake surface** and **River surface** immediately toggle rendering independently; **River channels** and **Lake basins** change terrain on Generate. Feature buttons focus the closest accepted headwater/mouth or the selected lake. Scroll the control panel to reach them. The map draws accepted river courses in cyan and supports deeper nested levels.

The overview keeps 2m samples and 16m world chunks. LOD defaults change to16m first threshold,1.2m distant error,0.08m near error and four bands. Column batching resolves layouts and surface-profile heights once per XZ column while retaining every Y field/material sample; arbitrary3D decorators still evaluate per sample. River segment bins avoid scanning the complete routed network at every point. These are evaluation/mesh optimizations, not adaptive source sampling.


### 010 validation notes (2026-10-27)

The development editor, editor import and27-class XML checks passed. Targeted diagnostics passed downhill/ocean endpoints, deterministic repeat planning, coastal/highland lake levels, one/32-worker output equivalence, cancellation, independent water visibility and previous field/LOD/collision regressions. Two seed exports each contain16641 valid2m columns with no top clipping; a wider31x31 study reaches-256.6m ocean depth. These are temporary development fixtures, not a new formal test suite.

On the unchanged0091m export, batching preserved all66049 height rows and162134 triangles, with local worker time19308→14435ms and samplingCPU159142→114421ms. Final010 LOD retained91584 of308286 source triangles in a256m-square study; the previous preset retained91630, so the additional LOD gain is small. Cached source fields match exactly. A384m-radius corrected010 run progressively filled136 patches in65.4s under concurrent test load before the final small LOD retune; larger/deeper content still costs more overall. Timings are dev-machine observations, not guarantees.
