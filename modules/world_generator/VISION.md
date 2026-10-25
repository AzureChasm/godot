# World Generator Vision

Status: vision discussed; architecture not yet agreed; no implementation started. `world_generator` is the proposed module name and provisional directory name.

## Purpose and scope

Build a reusable C++ Godot module for authoring and generating configurable worlds through a hierarchy of definitions. It must work independently of Astray and remain usable by other games. The full vision is a long-term direction; the first prototype's scope still needs joint agreement.

Support ongoing generation of an expandable world and generation within a supplied bounded space. Space assignment includes vertical enclosures. World content may include natural terrain, underground environments, floating land, vegetation, and structures. A game's particular sea level, altitude distribution, biomes, and gameplay rules are configuration or game content.

The target experience includes natural-looking surfaces, persistent destruction and construction, detailed generation near players, and progressively coarser distant views. The terrain representation, editing model, persistence mechanism, and division of these responsibilities among module components are not selected. The generator is not yet committed to implementing a complete physics or gameplay simulation.

Physical behavior and exceptions should be compatible with a game's rules. Astray-specific magic, skill levels, dimensions, and content must not be hard-coded into reusable module logic. Interesting content need not exist at every coordinate or altitude.

## Authoring worlds in the editor

The working authoring model has two concepts: containers structure and position nested regions; decorators supply modifications. The original World Generator, Ocean, and Continent are container/region-layouter concepts. Containers can recursively contain smaller containers. Terrain formation, vegetation, structures, and other modifications are decorator behaviors, replacing the earlier terminal-formation requirement.

The unmodified base is a flat surface at world-space `Y = 0`, with air above and solid below. A container completely below zero starts solid; one completely above starts air. Decorators modify this shared world-space base. This does not select the internal terrain representation.

Decorations apply throughout a container's descendants. A forest rule can span hills and plains produced by other decorators. Descendants can add decorations, modify or replace inherited ones, or suppress them through the antidecorator concept. An island can thus contain a clearing and a birch forest. Exact targeting and inheritance precedence remain open.

Generation order is adjustable with sensible defaults. The current proposed sequence is terrain, vegetation, then structures, with fauna potentially later. Later decorators can modify earlier results: vegetation may edit terrain, and structures may edit both terrain and vegetation. One decorator can group several operations. Exact priorities, ordering contracts, and affected-content reconciliation remain under discussion in [the architecture draft](ARCHITECTURE.md).

Each completed decorator operation must leave a consistent world, and its proposed changes must be validated before execution. Flora and structures may cross region boundaries, but must obey restrictions throughout the affected area. A building's foundation must not destroy or completely block a river; a watermill requires a deliberate compatible placement. Invalid placement may try another building. Restricting containers are an option under consideration; hard limits versus exact-fill requirements remain open.

Adding ocean and continent children lets the author configure selection chances and size ranges independently. A 70% ocean chance means selecting Ocean 70% of the time, not targeting 70% ocean coverage. An ocean definition with a 10% selection chance can still dominate the visible world if its generated instances are much larger than continents. How eligibility and space constraints affect the selection process remains open.

A child's minimum required size may be computed from its descendants. How that minimum is calculated remains open.

The editor tree contains reusable definitions, comparable to classes. When the World Generator decides that a space contains an ocean, it creates an ocean instance from the Ocean definition. That instance divides its assigned space and creates instances from its child definitions, such as deep sea, coral reef, or abyss. This continues down the hierarchy. The exact C++ types and whether generated instances are Godot scene nodes remain undecided.

### Shape and neighboring regions

The parent chooses what belongs in a place and an approximate size. The child determines its own shape: selecting an ocean of roughly a given size does not let the parent prescribe its entire coastline. The child's shape must also work with the position and enclosure it receives. How incompatible shape and space requirements are resolved remains open.

Neighboring regions must meet coherently, but explicit entry points are not a confirmed requirement. The user is unsure whether they are needed. We will revisit the representation of boundaries and connections when concrete generation examples require it.

The world has a starting point. When a new biome is added, it is responsible for creating its transition to the existing world. Transitions are configurable generation features: an ocean and continent might meet through a beach, cliffs, or another arrangement. How a new region handles multiple existing neighbors remains open.

**Under consideration:** a pool of possible transitions between two biome definitions, procedurally generated transitions, or a combination. No transition implementation has been selected.

**Assistant proposal:** let a transition rule describe both eligibility and a way to generate the result. A beach rule could use slope and width parameters to generate a varied coastline rather than place an identical premade beach. A seeded choice among compatible rules could combine authored control with procedural variation. Rules based on material, elevation, or other shared properties might later reduce the need to configure every biome pair separately. These are proposals to evaluate, not agreed architecture.

The relationship between a newly added biome and deterministic generation needs care: the transition should not change merely because a player approaches from the other side. A repeatable logical generation order or a consistent boundary agreement are possible approaches; neither is chosen yet.

### Combining terrain features

Features can be layered. Hills applied to a grassy plain should produce grassy hills; applied to a desert with suitable configuration, they should produce dunes. The result depends on both the underlying environment and the applied feature, rather than blindly repeating the same shape everywhere.

Vegetation and constructed structures come after the terrain composition in the author's intended workflow. Rivers might also come afterward, but their placement in the process is tentative. Layer order, affected areas, and the resolution of conflicting changes remain open. Partitioning into regions and layering features are both part of the vision; not every child needs to represent an exclusive spatial subdivision.

## Flexible and reusable implementation

The module should have interchangeable components so generation behaviors can be combined or replaced. Exact classes and boundaries are undecided. We will identify useful responsibilities through concrete examples before designing the class hierarchy.

The editor tree expresses reusable generation definitions; generated regions are instances. How those definitions become an executable generation description remains open. Terrain storage, surface generation, streaming, persistence mechanisms, and database technology have not been selected. Natural-looking terrain does not yet prescribe an internal representation.

## Reproducibility and optional world evolution

**Confirmed:** the same generation inputs must produce the same output, including use of a world seed. Generation should not produce a different original landscape just because exploration or loading happens in another order. The complete set of inputs and the treatment of later generator changes still need definition.

**Optional future direction:** generation or simulation over time could make the world feel alive. This is a nice-to-have, not a requirement for the first generator milestone. No particular effects, simulation model, or simulation of unloaded regions have been agreed.

**Assistant proposal:** distinguish reproducible initial generation, persistent world changes, and optional ongoing simulation. If simulation is added, elapsed world time and simulation state become explicit inputs. This would let a world change while preserving a clear meaning for "same input, same output." The interfaces are not yet designed.

## Architecture work still ahead

The working container/decorator topology and ordering discussion are recorded in [the architecture draft](ARCHITECTURE.md). They complement the definition/instance distinction below. The full architecture is not yet approved for implementation.

**Initial assistant proposal, not agreed:** separate three responsibilities before choosing classes:

- **Definitions:** reusable authoring rules such as Ocean, its child biomes, selection weights, and size ranges.
- **Generated regions:** particular ocean or biome instances, with identity, assigned space, chosen shape, and child relationships.
- **Materialized world:** terrain, meshes, collision, and placed objects produced for the areas currently needed. The exact storage and streaming model remain open.

For example, a single Ocean definition could describe multiple generated oceans. Their region descriptions need not disappear merely because their visible terrain is unloaded. How much region state is retained, regenerated, or saved needs a separate decision. These responsibilities do not yet prescribe separate subsystems, Godot base classes, or a generation pipeline.

We must jointly define responsibility boundaries, data flow, public APIs, generation inputs, coordinate and space semantics, transition behavior, reproducibility, terrain representation, lifecycle, persistence interactions, and the first prototype's visible success criteria. The number of classes and exact abstractions should follow those decisions.

No prototype or implementation scaffolding is authorized until the user and assistant are both satisfied with the planned architecture. Discuss strengths and weaknesses of suggestions candidly. Keep the resulting C++ understandable through focused explanations and a guided review.

Implementation will happen on a new feature branch. After the user reviews the prototype in the editor and we make the initial adjustments, create its initial implementation commit. Establish the module's automated testing structure afterward. Builds and appropriate manual validation still accompany the prototype.
