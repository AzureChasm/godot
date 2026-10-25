# World Generator Architecture Draft

Status: joint design in progress, not approved for implementation. The working authoring model is containers and decorators. Concrete C++/Godot types, data contracts, terrain representation, and prototype scope remain undecided.

## Current focus: terrain first

The high-level representation direction is accepted: sparse signed scalar terrain plus material data, separate terrain/objects, chunks, and multiple LODs. Trees require arbitrary cut locations. Storage, meshing, resolution, and object-cutting choices remain open in [the representation discussion](REPRESENTATION.md); no backend is selected.

**Confirmed priority:** terrain comes first. Structure and city generation are later work. Preserve those ideas below without continuing their detailed design now. The next architecture discussion should concentrate on terrain representation, container scope, terrain decorators, ordering/composition, and an editor-reviewable terrain milestone.

**Assistant scope proposal, not yet agreed:** use a bounded editor preview to establish the flat solid/air base, nested containers, deterministic terrain decorators that can add and remove ground, configurable ordering, and visible three-dimensional results. This should exercise hills, a cave, and floating land without selecting a complete biome catalogue or implementing vegetation, buildings, fauna, city layout, or time simulation. Collision, interactive editing, persistence, and streaming inclusion need explicit milestone decisions. The bounded preview does not replace the long-term expandable-world requirement.

## Base world

The concrete chunk data and staged edit lifecycle proposed below are under discussion. They do not authorize implementation.

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
