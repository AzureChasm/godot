# World Generator Architecture Draft

Status: user approved the current architecture for a first prototype. The container/decorator terrain slice is implemented on `feature/world-generator-prototype` and has been reviewed with initial corrections; the user authorized its first implementation commit. `PROTOTYPE_PLAN.md` defines this slice; `README.md` distinguishes implemented behavior, provisional choices, and deferred long-term design.

## Current focus: terrain first

The high-level representation direction is accepted: sparse signed scalar terrain plus material data, separate terrain/objects, chunks, and multiple LODs. Trees require arbitrary cut locations. Storage, meshing, resolution, and object-cutting choices remain open in [the representation discussion](REPRESENTATION.md); no backend is selected.

**Confirmed priority:** terrain comes first. Structure and city generation are later work. Preserve those ideas below without continuing their detailed design now. The next architecture discussion should concentrate on terrain representation, container scope, terrain decorators, ordering/composition, and an editor-reviewable terrain milestone.

**Approved first slice:** use a bounded editor preview to establish the flat solid/air base, nested containers, deterministic terrain decorators that can add and remove ground, configurable ordering, and visible three-dimensional results. This should exercise hills, a cave, and floating land without selecting a complete biome catalogue or implementing vegetation, buildings, fauna, city layout, or time simulation. Static collision is included; interactive runtime editing, persistence, and streaming are deferred. The bounded preview does not replace the long-term expandable-world requirement.

## Base world

The first slice implements bounded synchronous candidate generation; the production chunk/edit lifecycle below remains a design direction. It is not a claim that streaming or concurrent atomic publication has been implemented.

**Confirmed:** before decorators act, the world has a flat surface at world-space `Y = 0`. Everything above is air and everything below is solid. No vegetation or structures are present initially.

A container entirely below zero starts completely solid; one entirely above zero starts entirely air. A container crossing zero contains the surface. Each container samples the same world-space base: moving a container does not create a new local zero-level surface.

Decorators can change this starting state. An underground decorator could carve caves; an airborne decorator could add floating land. The flat base is a generation rule, not a decision to use heightmap storage. The terrain representation and surface extraction method remain open.

## Proposed terrain chunk data and edit lifecycle

### Candidate class responsibilities for joint review

The user proposes extensible container and decorator interfaces, concrete terrain decorators, a chunk-backed data model, and a view that represents it in the world. The following is an assistant refinement, not an approved class/API design.

| Candidate role/name | Responsibility | Possible Godot relationship |
| --- | --- | --- |
| RegionContainer | Author nested scope, child layout/selection, enclosure, and attached rules. Extensible base with shared behavior. | Node3D is a candidate for authoring in the scene tree. |
| WorldDecorator | Common contract for configurable generation/modification behaviors; terrain implementations can raise, carve, or paint. | Stored in a container property list, never a child node. Abstract Resource is the proposed mapping. |
| TerrainData / TerrainChunk | Addressing, field/material payloads, residency, revisions, and read access. | Prefer data-focused C++ types; editor exposure only where useful. |
| TerrainEditCoordinator | Turn requests into staged edits; enforce validation, conflict/order, budgets, and coherent publication. | Internal C++ service with an eventual engine-facing entry point. |
| TerrainMesher | Derive geometry from a specified terrain snapshot; support the selected meshing/LOD contract. | Internal replaceable backend contract, not the chunk authority. |
| TerrainView | Present accepted geometry and coordinate required collision/LOD integration. | Node3D is a candidate scene/editor adapter. |

These names describe responsibilities; implementations may combine or split them after interface review. Collision building/publication may need its own adapter rather than being hidden inside a rendering-only view. Persistence remains a separate responsibility to design.

An extensible base class can already provide a C++ interface and common state. Do not add a separate pure `IContainer` hierarchy only to enable subclasses; consider a pure interface when unrelated implementations actually need the same contract. Configurable presets can express Ocean versus Continent content without necessarily requiring an engine subclass for each biome. Different layout algorithms may justify subclasses or replaceable layout strategies; this choice is open.

**Assistant flow proposal:** editor container/decorator definitions -> immutable generation inputs -> coordinator and constrained staging context -> candidate chunk changes -> validation and derived-output builders -> coordinated data/view/collision publication. Decorators do not directly mutate live published chunks or scene meshes.

The distinction between authoring and generation must remain explicit. Godot's active scene tree is not generally thread-safe. Background work should consume appropriately owned snapshots/data rather than mutate editor nodes or shared configuration resources. Resource reference counting alone does not make concurrent modification safe.

References: [Godot object registration](https://docs.godotengine.org/en/stable/engine_details/architecture/object_class.html), [Resources](https://docs.godotengine.org/en/stable/tutorials/scripting/resources.html), and [thread-safe APIs](https://docs.godotengine.org/en/stable/tutorials/performance/thread_safe_apis.html).

**Confirmed authoring model:** only region containers form the world-definition tree. Decorators are entries in each container's list property, not child nodes or attachment nodes. The earlier attachment-node suggestion is superseded. **Assistant mapping proposal:** use reusable Godot Resource objects for decorator list entries. A reference points to a configuration object rather than embedding a scene-tree child. Shared resource edits and per-container overrides require explicit semantics; inheritance must not mutate an ancestor's shared configuration accidentally.

This section is an assistant proposal for joint review. Exact types, ownership primitives, grid dimensions, and failure/publication mechanisms are not selected.

### Authored regions and technical chunks

Containers describe authored regions; chunks partition terrain storage and work. They are independent: a continent can span many chunks, and a chunk can intersect several containers. Container layout must not depend on the camera's current LOD. Terrain sampling resolves the relevant effective decorator rules at world locations.

Proposed chunk addressing uses integer coordinates, with 64-bit components as a candidate for large worlds, and a detail level where needed for derived data. Local sample positions are computed relative to a chunk/grid origin. World extent limits, negative-coordinate division, transforms, and floating-point rendering origins need explicit decisions.

### Chunk contents and ownership

| Concept | Proposed contents or responsibility |
| --- | --- |
| Chunk address | Grid coordinates and relevant detail level, separate from container identity. |
| Terrain data | Signed field and material channels, represented implicitly/uniformly where valid or as compact dense sample arrays where needed. |
| Data revision | Identity/version of the published terrain contents, plus enough provenance to validate derived work. |
| Derived output | Meshes, collision, and coarse LOD products built from specified terrain revisions. |

The finest authoritative terrain and its edits are conceptually distinct from coarser rendering/streaming caches. A coarse mesh is not a replacement for persistent fine-detail changes. Finest detail need not be resident for the entire world.

An absent chunk means data is not resident, not that the region is air. Queries may resolve the procedural base/decorators and saved changes or request residency. The nonblocking/blocking query contract is still to design.

Sample boundaries require one consistent global lattice/ownership rule. Meshing may read neighboring samples; an edit near a border must invalidate all dependent meshes, not just the chunk containing its center. Padding/halo width, overlapping caches, and exact sample-versus-cell dimensions depend on the chosen mesher.

**Recommended ownership model for discussion:** published chunk data is read-only. An edit prepares replacement data for affected chunks while readers retain the previous published revision. Initially, copying only affected chunk payloads is a candidate simple implementation; smaller copy-on-write bricks or other optimizations require evidence. Staging memory and retained readers count against the runtime budget.

### One edit crossing a chunk boundary

Consider digging a spherical hole spanning chunks A and B:

1. Describe the desired edit independently of resident mesh nodes: operation shape, world position/extent, material behavior, presentation intent, and deterministic inputs where relevant.
2. Resolve the complete affected region and required neighboring/read data. Capture the revisions used by the operation, including relevant constraints.
3. Prepare proposed field/material changes and required dependent updates in staged data. Validate the proposed combined result before publishing changes to the live world; intermediate calculation may be necessary for validation.
4. Prepare required meshes, collision, and boundary/LOD updates for the agreed publication policy. The exact safe engine synchronization mechanism is undecided.
5. Check that the input revisions are still current. If an overlapping edit changed them, re-evaluate or retry according to an explicit deterministic conflict policy; never publish stale data over newer changes.
6. Publish the operation's coherent result at a controlled synchronization point. If preparation or validation fails, keep the previous published world. Multi-chunk publication and failure recovery need a concrete design.
7. Mark the accepted edit/state for persistence and release old data once no readers need it. Save durability, crash recovery, and generator-version compatibility are separate unresolved contracts.

The user requires consistency after every completed operation. An instant edit coordinates all chunks in its logical change. A progressive spell consists of intentional consistent steps. Background jobs may complete in any physical order, but conflicting logical edit order must be explicit and deterministic for the same inputs. Only redundant derived-output rebuilds may be coalesced without changing edit semantics.

Published terrain, its active collision, and visible output need a defined version/publication policy; merely swapping a data pointer does not prove they are synchronized. Large operations must account for preparation latency and peak memory, without silently converting requested instant effects to progressive ones.

### Proposed responsibility boundaries

- Terrain data/store: addressing, residency, versioned chunk data, and query access.
- Edit coordinator: candidate edits, validation, affected regions, conflict handling, budgets, and publication.
- Surface/collision builders: derived output from specified immutable inputs.
- Persistence: accepted modifications and reconstruction metadata, independently of disposable meshes.
- Godot integration: editor properties, scene objects, and required engine synchronization.

These are responsibility boundaries, not a mandated class per bullet. Registration, ownership types, task APIs, and editor exposure will follow the agreed contracts.

## Containers

**Confirmed:** the root is a region container. With an explicit assigned area, it generates only within that area. With no area, its generation domain is unbounded. The shape representation and bounded-domain edge behavior still need design. Unbounded generation uses the chunk/LOD demand model rather than eagerly materializing the entire domain.

Root boundedness must be explicit; do not silently encode "no area" as a zero-sized area. Whether the API uses an optional shape, a bounds mode, or another representation is not selected. Root-domain restrictions are distinct from ordinary child-region boundaries, which earlier requirements allow flora and structures to overreach subject to constraints.

The root's children are region container definitions, recursively. Containers structure, size, shape, and position nested regions and have a decorator-list property. Ocean and Continent are examples of container definitions. No fixed conceptual nesting depth is assumed.

A container's boundary and enclosure structure generation scope. Terrain modifications belong to decorators. How the parent allocates space while respecting a child's chosen region shape remains open. Runtime termination, minimum-size handling, and allocation validation also need design.

## Decorators

Decorators are list-property entries on containers and supply modifications: terrain shaping, carving, materials, vegetation, structures, and potentially fauna generation. There is no longer a required terminal Terrain Formation role in the working model. Terrain formation is one kind of decorator behavior.

A shared authoring concept does not require every decorator to perform identical internal work. Terrain changes and object placement need appropriate inputs, outputs, and dependencies. Fauna placement does not yet specify a runtime animal or ecosystem simulation.

## Cascading scope and local rules

Containers behave like nested HTML-style containers for authoring purposes. A decoration on a container applies throughout its descendant regions, and descendants may add their own decorations. This is region-tree inheritance, not necessarily C++ class inheritance or a commitment to implement CSS syntax.

A forest island can contain a clearing that suppresses forest decoration and a birch forest that modifies or replaces it. The user suggests antidecorators for suppression; concrete types and target selection remain open. Adding, modifying, replacing, and suppressing decorations are distinct intended capabilities, with exact semantics still to design.

**Assistant proposal:** resolve effective inherited rules before evaluating decorators. Suppression then prevents generation instead of generating content only to delete it. Scope is local to the affected subtree; siblings retain their own rules. Generation-rule suppression is not permission to erase saved player modifications.

Rule identity, categories, partial settings, precedence, and whether a deeper descendant can restore a suppressed rule remain open. A candidate nearest-explicit-setting rule is still only an assistant proposal.

## Generation order and combined operations

**Confirmed:** generation order must be adjustable, with sensible defaults.

**User proposal, supported by the assistant as a candidate default:** run decorators in a logical forward order. Later decorators can read and modify earlier results. A phase describes a decorator's main purpose, not exclusive ownership of one kind of world data.

| Default order under discussion | Main work | Permitted earlier-result edits in this proposal |
| --- | --- | --- |
| Terrain | Form the initial landscape. | Base solid/air state and existing terrain modifications. |
| Vegetation | Place trees and plants. | Terrain, for example around roots. |
| Structures | Place buildings and other constructions. | Terrain and vegetation, for foundations and clearing. |
| Fauna, proposed later stage | Place or spawn animals using the resulting environment. | Exact capabilities and ordering remain open. |

A structure decorator can group several operations at its logical position: inspect a site, prepare terrain, remove or alter vegetation in the footprint, and place the structure. Its terrain edits need not be moved back into the initial terrain phase. This updates the earlier assistant suggestion that every foundation edit should be scheduled there.

**Confirmed:** the world must be consistent after each completed decorator operation. An operation must be checked for validity before execution. Invalid placement may lead to trying another building; the fallback sequence and other alternatives are not yet specified.

**Assistant interpretation:** a combined placement is one logical operation whose proposed terrain, vegetation, and building changes are checked together. Any required reconciliation of affected earlier content belongs to that operation's successful result, not to a later global repair pass. Failure should leave the previously consistent world intact. Whether this uses staged changes, rollback, or another mechanism remains undecided.

**Assistant proposal:** identify each operation's read data, proposed writes, and complete affected area. Later terrain edits may affect nearby vegetation beyond the building footprint; this must be included in validation and any adaptation/removal policy. Re-running whole earlier generation passes could undo intentional later changes, so repair/revalidation needs a defined scope. Exact consistency invariants, presentation/cache synchronization, failure handling, and configurable physics exceptions remain to design.

Scope resolution and execution order are different concerns: an inherited forest rule can be authored on an ancestor but evaluated after descendant terrain modifiers. Local suppression can determine which inherited decorators participate before placement.

The phase list, numeric priority direction, allowed reordering, dependency validation, and stable tie-breaking remain undecided. The same inputs must produce the same results regardless of actual task scheduling or exploration order. Priority changes intentionally change generation inputs.

## Placement restrictions and protected features

**Confirmed:** flora and structures may overreach their originating regions. Permission to overreach does not permit violating world restrictions. A house foundation must not destroy or completely block a river simply because the house was selected for that site.

A watermill is a special interaction case, but must still preserve the river rather than accidentally block it. The exact flow requirement and how we validate it have not been specified; this does not commit us to a full fluid simulation.

Validation must consider the proposed result, including terrain preparation and vegetation edits, rather than only whether a building's object bounds fit. It must cover affected neighboring space as well as the originating region. Relevant constraints must be available for that space; how this works across unloaded areas remains open.

**Future structure-generation ideas, deferred:** filling an assigned city space is a desirable building feature. Buildings may need vertical freedom rather than an exact fixed-height box. Adjacent buildings could reuse an existing wall as a finished boundary to avoid duplicate construction and save resources. Exact footprint/height rules, wall ownership, and editing behavior are not designed. Restricting containers, exact-fit generation, and river constraints remain future structure topics; a simple bounding box alone does not express river protection.

The selection/retry policy remains configurable design work. Trying another building is the user's example; another site, adaptation, or skipping placement are assistant-suggested alternatives, not yet selected defaults. Any retry process must retain the deterministic generation contract.

## Example authoring structure

Only containers appear as nodes in this world-definition tree:

```text
Root region container [assigned area, or unbounded]
├── Continent container
│   ├── Hills container
│   └── Clearing container
└── Ocean container
```

Their Inspector properties can describe decoration independently of the child tree:

```text
Continent.decorators = [Hills, Forest]
Clearing.decorators  = [SuppressForest, FlattenTerrain]
Ocean.decorators     = [OceanFloor]
```

This notation illustrates list properties, not implementation syntax or final class names. Inherited effective rules and operation order are resolved separately from tree membership and local list storage. Decorators may be reused across containers if desired; override processing must preserve local scope.

## Definitions, instances, and materialized output

Reusable authored definitions, particular generated region instances, and visible world output remain separate conceptual concerns. One Ocean container definition can describe many generated oceans. Meshes, collision, and objects represent the currently needed portions of their results. Retention, regeneration, and saved-state boundaries remain to design.

## Remaining decisions

- Decorator input/output contracts, terrain representation, and composition operations.
- Ordering phases, priority direction, dependency expression, and deterministic tie-breaking.
- Inheritance targets, modification/replacement rules, and suppression/restoration behavior.
- Grouped decorator operations, failure behavior, and reconciliation of earlier content after later edits.
- Container boundaries, falloff, biome transitions, size constraints, and bounded generation.
- Terrain queries, rendering, collision, streaming, level of detail, persistent changes, and optional simulation boundaries.
- C++/Godot types, lifecycle, and prototype success criteria, followed by joint architecture agreement before implementation.

## Session handoff 2026-10-25

We stopped after agreeing the high-level terrain representation. Next discuss the smallest editable terrain feature and target performance budgets, then sample spacing/precision, chunk data, surface extraction, LOD boundaries, and decorator data contracts. No numeric resolution or chunk size has been selected. Finish the architecture and prototype scope together before creating an implementation branch or writing a prototype. Structure/city generation remains deferred.

## Terrain-profile prototype extension (2026-10-25)

`WorldTerrainDecorator` now provides shared tunable surface-profile controls, with plains, hills, mountains, sea and deep-sea subclasses. Each compiles immutable noise settings into a modifier; the chunk field and meshing contracts are unchanged. Profiles default to replacement at priority -100, before sphere carving. Add/remove composition is explicit. These are topographic starting operations, not a restriction of the world representation to heightmaps. The standalone terrain gallery compares them with optional 3D cave carving and a visual sea-level guide. Transition blending, water simulation and procedural container allocation remain deferred.

## Fixed chunks and parent allocation extension (2026-10-25)

The user selected fixed 16 m world chunks and requested optimization plus an unbounded, weighted-biome demo with transitions. Internal `TerrainSection` arrays subdivide those chunks; spacing snaps to a whole-chunk lattice. A halo supports sampled shading gradients and indexed surfaces. Pure column profiles can reuse XZ evaluations without restricting the 3D field representation; sphere edits use the general path.

`WorldRegionLayout : RegionContainer` owns equal-sized, vertically extruded XZ allocations. Child containers are weighted reusable definitions; their immediate box/position is superseded by the parent allocation, while manually authored descendants keep relative scopes. Layouts can nest. Immutable template programs evaluate demanded logical instances; no infinite scene tree is built. Selection hashes seed and signed cell coordinates, independent of preview movement or discovery order. Current per-instance template noise patterns repeat.

Smoothstep weights blend up to four child signed fields across configurable transition bands. The full child program is grouped at the layout's priority -100; its internal ordering is local, not globally interleaved with ancestor operations. Ancestor decorations are evaluated once; later ancestor operations can modify the blended result. This is a provisional grid-allocation strategy, not the final organic region-shape algorithm. Materials still select the strongest cell. Pair-specific beach/cliff rules, material mixtures, variable sizes, vertical layout, streaming, LODs and persistent edits remain open.

Generation still rebuilds a finite preview synchronously. Stage timings and local benchmark results are in README.md; the new demo moves that request through the unbounded definition in 16 m steps. Automatic editor generation remains disabled.

## Voronoi allocation and independent camera (2026-10-25)

User requested organic biome outlines, configurable parent allocation, multi-cell Voronoi regions, and arrow keys that move the camera rather than immediately replacing the terrain beneath a recentered view.

`WorldRegionLayout` now offers grid and grouped Voronoi strategies. Deterministic fine sites stay within their lattice cells; their owners are the nearest sites of a coarser Voronoi partition with spacing scaled by sqrt(cells_per_region). This assigns a union of fine polygons to one logical region. Its weight-selected biome and template origin are shared across the union. Fine-cell proximity weights combine by region before program evaluation. A 5x5 site search is complete for bounded jitter and blend support capped to half a fine cell. Axis stretch acts in allocation space. This is 2D extruded allocation, not a full 3D Voronoi generator. Cell counts are approximate and connectivity is not guaranteed. Hard region diagnostics return both fine cell and owner IDs; material blending remains deferred.

The demo separates camera focus from loaded preview center. Arrows pan the camera continuously. The terrain window refreshes only after a movement pause or explicit Generate; native publication never changes camera transforms. Automatic refresh is optional, remains synchronous, and rebuilds a finite chunk-aligned window. The diagnostic map shows internal cell borders and outer region borders independently of the 16 m storage grid. Earlier notes describing square-only allocation and arrow-triggered recentering are superseded by this extension.

## Parallel generation and staged publication (2026-10-26)

User explicitly requested multithreaded terrain generation off the main thread, configurable view distance, and a responsive menu. This supersedes the earlier synchronous-preview implementation notes.

`TerrainView` captures node/resource configuration into an immutable request. A dedicated `TerrainBuildJob` coordinator performs validation/cache planning and dispatches worker-pool section tasks, then waits away from the main thread. Each callback owns one pre-sized output slot; the outer COW container is detached before dispatch, never resized concurrently, and shared compiled modifiers are read-only. Workers produce field arrays, indexed surfaces and collision faces. They aggregate by stable section order, making output independent of scheduling. Completion/progress/cancellation use atomic acquire/release handoffs; no scene objects or server APIs are accessed by generation callbacks. Actual worker IDs and a main-thread-violation diagnostic are recorded.

The main thread polls completion and registers at most 1024-triangle pieces under a soft frame budget. Hidden staging has collision layer/mask zero; only a complete latest candidate replaces the old visible/collidable preview. Old nodes retire incrementally. Physics/render server calls and the final activation cannot be preempted; this is not production physics-tick atomicity or a hard frame-time guarantee. More small mesh pieces increase draw calls/node counts.

Requests coalesce to one active and one latest pending immutable snapshot. Superseded jobs cooperatively cancel; cancelled/failed results never replace published data. Node destruction cancels and joins callbacks before freeing their data. `generate_preview` returns acceptance, with completion/failure signals and progress polling. The UI adds radius/spacing/worker/collision controls, Cancel, progress, scrollable layout and incremental diagnostic-map work. Whole-window regeneration remains; incremental streaming/LOD/cache reuse are deferred. Default safety budgets are 16 million halo-inclusive samples and 2 million triangles.


## Surface carpet, nesting and initial mesh LOD (2026-10-26)

Confirmed direction: surface biomes form a thick, potentially layered carpet
following a reference surface. Caves, pits and ravines can breach it and expose
underground content. Regions remain recursively nestable; a ravine can belong to
a bounded subregion. Full 3D allocation is particularly useful underground and
remains a future allocation strategy. Moving floating islands are unresolved;
generating their content and owning their motion should remain separate concerns.

Prototype increment: base-elevation Resources resolve as root-to-leaf metadata,
in list order (absolute replacement or relative addition), before compiling
profiles/ravines. Bounds and explicit spheres retain spatial coordinates.
Metadata alone does not change the Y=0 base field; a flat profile supplies ground.
Nested scopes intersect; layout template instantiation preserves descendant
scopes. Ravines carve an upward-open elliptical/meandering cut with a finite
bottom. Strata assign deep material using the incoming field before carving;
this is not yet a continuously queryable reference surface or 3D underground layout.

Distance LOD uses worker-side meshoptimizer simplification, not mixed sample
lattices. Section topological borders stay fixed; distance to the section AABB
selects full detail or target triangle fractions 1/4 and 1/16, with absolute
error caps. A per-section edge-incidence check rejects changed open/nonmanifold
edges and falls back to the original mesh. Caller supplies world-space focus per request. Collision uses the
same reduced geometry. Sampling/base extraction remain unchanged; simplification
adds worker work but reduces geometry registration and rendering. No per-frame
LOD switching, streaming reuse or transition-cell mesher is claimed. Optional
meshoptimizer absence fails enabled LOD requests rather than silently ignoring it.


## Cumulative nested demo and instance identity (2026-10-26)

User clarified that every newest iterative demo must preserve earlier features
and controls for interaction review. Demo scenes now use chronological
`NNN_descriptive_name` names, matching scripts, preserved UIDs and latest-as-F5.
004 supersedes the fixed lowland/highland example with the complete interactive
world and actual weighted nested layouters through four levels.

TerrainModifier::apply_seeded is a backward-compatible immutable context hook:
default implementation calls apply. Layouts compose parent identity, their seed
and owner coordinates and pass the derived uint32 identity to child operations.
Nested child selection includes parent identity; geometry sites remain defined
by the layouter's local settings. Fine cells sharing an owner share identity.
Ravines use it for optional occurrence and position/rotation/size/bend variation.
There is no shared RNG or discovery-order state. Probability/variation defaults
preserve existing explicitly authored ravines. Sizes remain in metres; demo
resources intentionally choose decreasing dimensions at increasing depth.

get_region_at accepts optional parent instance_seed and returns the selected
instance_seed for deeper queries. Diagnostics convert positions into template
coordinates exactly as compiled evaluation does; they remain authoring diagnostics,
not queries of a previously published snapshot. Existing one-argument callers
retain the root/default-zero behaviour. This does not implement 3D Voronoi.


## Biome envelopes, terrain variety and CPU defaults (2026-10-26)

User requested all-core worker defaults, more view distance, less uniform nested
terrain, lower mountain outskirts/higher spiky interiors, rarer ravines and no
floating-sphere demonstration. 004 now has twenty profiles (three formation
subregions per biome plus five inherited fallbacks), 256x128x256m default coverage,
rare4%/8% cut settings and grounded profile-union outcrops instead of an additive
sphere. Floating-island generation remains deferred.

TerrainEvaluationContext carries stable instance seed, original world position
and inherited biome depth. apply_context defaults to the existing seeded hook;
layouts copy context for descendant programs. The optional biome_edges setting
uses a bounded5x5 nearest-matching/foreign-site distance-gap estimate, smoothly
capped at0.35 cell widths (minimum axis scale). Same-biome owner edges do not
reset its envelope. Profiles opt into world-space noise, interior elevation/relief
ramps, octave roughness and ridge sharpness. This is not a global connected-
component ID/centroid solution: diagnostic owner identities remain for allocation
and seeded local operations. Top-level map presentation hides white owner borders
between matching biome types while retaining fine-cell diagnostics.

Immutable compiled objects receive unique cache IDs. Fixed4096-entry thread-local
XZ caches reuse profile heights and boundary estimates across Y samples; entries
also check coordinates, inherited depth or seed/definition as relevant. No pointer
reuse or stale-request hits, shared writes, task-order RNG or server access occurs.
Worker count is OS logical processor count by default; positive explicit limits
remain, capped by the shared pool. Terrain group tasks use its full/high-priority
queue instead of the pool's limited low-priority allocation. Main-thread scene/
physics publication stays budgeted and generation remains worker-only.


## Structural mountains and layered landforms (2026-10-26)

User delegated terrain technique/preset/tree authoring to the assistant, with
qualitative visual feedback instead of identifying parameter fixes. Optional
reference photos guide appearance. Architectural participation and push permissions
remain unchanged; this is authority to iterate within the agreed generator model.

005 preserves cumulative features and keeps004 as a comparison. Nine new concrete
WorldTerrainDecorator classes share the sampled-field/worker/LOD contracts. A
MountainRange profile builds deterministic summit points, compact mountain bodies
and ridge/saddle connections across adjacent world tiles. The graph is evaluated
locally with shared endpoints; biome envelopes provide scope/falloff. It is not
an erosion simulation or exact region-polygon graph fitting. Existing noise-based
Mountains remains for compatibility/comparison, but does not generate005 mountains.

ADD_RELIEF shifts the incoming field by the local profile height and preserves
material. It ignores the container datum in the shift so base height is not added
twice. Mountain parents own the macro graph; nested children add shallow foothill,
valley and terrace layers before carving. Additional plateau/mesa/dune/basin/
escarpment profiles demonstrate other landforms. Current layouters are sufficient;
no speculative new container interface is added simply to grow the type catalogue.


## Cached-surface diagnostics and evaluated summits (2026-10-26)

User requested a height map from generation to evaluate the actual composed
output. TerrainHeightmap exports an optional CSV from completed TerrainData on
the coordinator, before LOD/publication. It uses source-grid crossings, not the
analytic profile shortcut, and marks clipped/missing columns. Request provenance,
atomic file completion, cancellation and nonfatal diagnostic errors are explicit.
This is a top-surface diagnostic; the 3D signed-field representation is retained.

The optional NumPy/Matplotlib tool evaluates window-limited prominence, physical
apex-ring drops, slopes, residuals and altitude variation; it is analysis tooling,
not engine generation. Reports include inputs/thresholds/censoring. Synthetic
fixtures verify broad versus sharp peaks, unequal heights and known saddles.
Assess the complete scene with fixed paired inputs plus screenshots, and reject
flat/uniform results even if their spike count improves.

006 keeps005 as a baseline. Compatible graph parameters add rounded summit/crest
cross-sections, broader bodies, height contrast and dominant/secondary bands.
Irregular connections use a symmetric edge hash. Bounded per-worker tile caches
reuse immutable summit descriptors by unique compilation ID and world tile;
height changes cannot reuse stale entries. All earlier cumulative features remain.

## 2026-10-26: irregular region boundaries and review scale (007)

`WorldRegionLayout.boundary_noise` adds an optional two-scale seeded value-noise
warp in normalized layout space. The same query transform feeds ownership,
blend weights, local biome-depth estimates and authoring diagnostics. Grid and
zero-noise layouts preserve prior behavior. Per-worker memoization keys include
normalized input, seed and amount; immutable snapshots retain their own settings.
Centers remain lattice anchors rather than exact centroids of warped regions.

The native preview ceiling is16384 sections, with independent sample/triangle
budgets unchanged. Demo007 preflights the lattice/halo allocation and optionally
coarsens whole-window sampling to preserve the requested radius. It reports both
requested and effective spacing. This is a preview policy, not adaptive-distance
field storage, incremental residency or a production streaming implementation.

007 retains the complete006 hierarchy and controls, adjusts all five terrain
families, and adds a stationary1.8m capsule at X/Z=0. A post-publication one-column
field query positions its feet; collision generation is independent. The capsule
is a review aid, not a player controller. A close-up button changes the camera.
Paired maps for seeds2026/42 retain wide peak-height spread but flag more sharp
features than006: added roughness is an artistic tradeoff requiring review.

## 2026-10-26: incremental residency and river courses (008)

The user's movement-priority request advances beyond whole-window replacement.
`TerrainStreamer` owns desired/resident patch sets, a priority queue, one active
`TerrainView` job owner and staged outgoing views. Patches are64m XZ columns of
16m world chunks; vertical bounds come from the template. Near-distance priority
wins, with a forward-direction preference within nearby rings. Demand replans on
16m focus-cell changes or direction reversal. Outgoing work is cooperatively
cancelled; nearby missing work can preempt a distant active patch. Source mesh
positions never follow the camera. LOD refresh uses hysteresis when residency
approaches/recedes. This is an initial streamer with fixed source resolution,
not adaptive-distance sampling, persistence or vertical residency migration.

A rebuild compiles one immutable root program and captures generation settings
on the main thread. Every patch in that epoch shares it, so live resource edits
cannot change only newly loaded terrain. `TerrainView` still owns worker and
staged render/physics publication. Its internal C++ snapshot handoff bypasses
per-patch authoring compilation; public whole-window preview behavior remains.
Source arrays are released after patch publication, retaining the program for
field diagnostics. The template's triangle cap bounds resident output; nearer
patches can displace farther ones. Retiring views/candidates add temporary memory.
Low-priority WorkerThreadPool tasks leave capacity for higher-priority engine
work. Requested workers still default to logical CPUs; actual task concurrency
honours the engine pool's low-priority ratio. Publication uses a soft1ms budget
per active view and retirement proceeds through one outgoing view at a time.
These budgets reduce contention; they do not provide hard realtime guarantees.

`RegionLayoutModifier` caches bounded XZ layout plans per worker, keyed by unique
compiled identity, local coordinate and inherited instance seed. Plans include
selection/weights, instance origins and optional biome depth. Copying the plan
before recursion prevents child cache-slot replacement from corrupting its
parent. Full cached-field export of007 remains exactly equal to the saved source
lattice after this optimization. No mutable generation state is shared.

`WorldRiverDecorator` is a separate native operation: a downhill authored
centerline, deterministic meanders, channel cross-section and smooth bank cut.
It removes ground only and rejects invalid/uphill paths during compilation.
The renderer's water ribbon uses the same subdivided centerline. Optional XZ
bounds assign ribbon segments to resident patches by midpoint; a segment/width
can overreach at a residency edge. It is a static visual course, not automatic
catchment routing, hydraulic erosion, flowing fluid or arbitrary-path validity.
The demo captures separate immutable river render definitions on Generate.

008 keeps the cumulative hierarchy/controls, adds nine composed terrain biome
presets (14 total), two joining river courses and larger angular summit/ridge
ranges. Forest names describe ground formations only. Existing decorators remain
interchangeable; no Astray gameplay rules enter the reusable module. Source CSV
exports use a bounded batch with streaming disabled for reproducible evaluation.

## 2026-10-26: lakes, graded water and conservative mesh reduction (009)

`WorldLakeDecorator` forms a complete signed-field basin: bed, interior and
closed banks blended into incoming terrain, with a seeded elliptical shoreline.
Unlike the river's remove-only operation, basin formation can add grounded
terrain so standing water above sea level has a bed and banks. Default priority
-40 puts later river/cave cuts after it. Center Y is an explicit independent
water level, not a translated base plane or inherited metadata. Root009 examples
use a river-connected level near6m and an upland level72m. Scope and instance
ownership remain the caller's responsibility for nested generated water renders.
Water polygons share the native shoreline and clip to loaded patch boundaries.
The renderer can conform crossing river vertices to the standing lake surface;
this is static water geometry, not a volume/flow solver.

Rivers now have optional additional downstream slope, calculated from cumulative
horizontal length along the authored polyline. Both sampling and water ribbons
consume the same finalized meandering centerline. The demo matches the tributary
endpoint and connected lake level when applying grade changes. A downhill authored
path is still required; this does not discover a drainage network automatically.

Mesh simplification optionally includes the near band through lod_near_error;
zero remains the compatible default.009 opts into0.08m absolute error and keeps
existing fixed borders/topology validation. lod_max_level allows1..4 bands,
default2;009 uses3 with24m first threshold and0.5m distant base error. Source
sampling is unchanged and collision follows the reduced mesh. A controlled
128x256x128m all-near009 window reduced199499→54529 triangles, with exactly equal
cached height CSVs and unchanged outer boundary edges. Local worker wall was
5518→4934ms and accumulated publication33.3→11.1ms; these timings are not universal
speed guarantees. The tree has also grown, so whole-scene iteration timings are
not an isolated optimization benchmark.

009 retains008 streaming and all cumulative controls, expands to20 biome types,
109 formation leaves and332 tunable profile resources. Each direct formation
adds small relief and contour layers. Explicit biome datums create broad altitude
variation; plateaus/terraces/scarps add local variety. Profile menu names include
formation, kind and base/layer role. The speed slider directly drives arrow
movement without a modifier shortcut. G safely initializes the debug overlay;
streaming labels skip unloaded positions, query highest terrain crossings and
rebuild with frame yields. Grid lines remain a sea-level16m reference.

The extra distant band exposed a mixed cliff/ground case where a section-face
edge gained four global incidences despite passing an open-edge-only guard.
Validation now compares geometric face-edge incidence as well as topological
open edges. On a changed face, the simplifier retains the original triangle
collar and retries the remaining interior. Each retry protects another face,
so the loop is bounded to seven passes. Wholly planar sections allow their
common-plane interior to simplify while their outline remains fixed. This
prevents overlapping cliff edges without retaining dense interiors everywhere.

009 defaults to2m overview samples for faster residency filling;1m/finer remain
selectable. Heightmap comparisons use explicit1m sampling, including the new
export tool --spacing override. Small sub-sample cuts may be less visible in the
performance preset. The continuous field/decorator model is unchanged.


## 2026-10-27: climate hierarchy and drainage study

010 adds authored climate containers between broad land/ocean allocation and biome definitions. All previous terrain types, relief layers and nested cuts remain. Marine profiles deepen by inherited ocean-interior distance; the source enclosure expands to -288..224m. Climate names represent grouping presets rather than a temperature/moisture simulation.

`WorldRiverRouter` is a RefCounted planning service, separate from scalar carving and rendering. Main-thread capture compiles an immutable pre-river program; a dedicated coordinator builds a bounded, world-anchored jittered Voronoi dual graph. Worker-pool tasks project node/edge-midpoint terrain heights. Boundary-connected submerged nodes define ocean outlets; reverse Dijkstra selects outlet-directed paths. Highland source candidates are refined at roughly2m intervals against actual terrain, enforcing nonascending water and an incision cap. Edge noise preserves endpoints, with straight-edge fallback. Sources can join accepted trunk nodes. Rejected candidates are skipped. Accepted world-space paths become ordinary root-local river decorators in the origin-rooted demo, with no second slope/meander deformation. Callers using translated roots must convert coordinates themselves.

Planning uses a fixed finite study area independent of streamer focus. It is not an infinite watershed solver, rainfall/erosion model, or routing along the exact biome polygons. Its coarse top projection may miss thin roofs and small features. Coastal and highland lake suggestions use local heights, ocean proximity and exclusion around accepted rivers; basin creation remains an explicit later decorator operation.

Generation columns carry exact original world coordinates through nested translated scopes. Layout plans are resolved once per column, copied before recursion; weighted field/material accumulation retains scalar operation order. Surface profiles compute a single cached height for each column, then apply their original blend operation to every Y sample. Other3D modifiers retain scalar evaluation. Fixed-size67-element scratch arrays cover the bounded section halo and router's65-sample projections. River carving indexes immutable segments in XZ bins, preserving segment order and nearest-course semantics. Optional max_cut_depth limits scalar-field changes around planned channels; legacy zero leaves carving unrestricted.

Streaming can pause scheduling/publication while retaining old residents during plan preparation. Explicit regeneration cancels stale planning, captures current resource settings, and publishes only the latest request. Water visibility is independent of carving. This extends the existing immutable-request and main-thread scene/server publication model.
