# World Generator Architecture Draft

Status: joint design in progress, not approved for implementation. The working authoring model is containers and decorators. Concrete C++/Godot types, data contracts, terrain representation, and prototype scope remain undecided.

## Base world

**Confirmed:** before decorators act, the world has a flat surface at world-space `Y = 0`. Everything above is air and everything below is solid. No vegetation or structures are present initially.

A container entirely below zero starts completely solid; one entirely above zero starts entirely air. A container crossing zero contains the surface. Each container samples the same world-space base: moving a container does not create a new local zero-level surface.

Decorators can change this starting state. An underground decorator could carve caves; an airborne decorator could add floating land. The flat base is a generation rule, not a decision to use heightmap storage. The terrain representation and surface extraction method remain open.

## Containers

Containers structure, size, shape, and position nested regions. The original World Generator, Ocean, and Continent authoring concepts belong to this role. Containers can contain smaller containers recursively, without a fixed conceptual nesting depth.

A container's boundary and enclosure describe where its content may generate. Terrain modifications belong to decorators. How the parent allocates space while respecting a child's chosen region shape remains open. Runtime termination, minimum-size handling, and allocation validation also need design.

## Decorators

Decorators supply modifications: terrain shaping, carving, materials, vegetation, structures, and potentially fauna generation. There is no longer a required terminal Terrain Formation role in the working model. Terrain formation is one kind of decorator behavior.

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

**Assistant proposal:** each operation group identifies the world data it reads, what it changes, and its affected area. Group preparation and validation should avoid leaving a cleared site with no building if placement fails; rollback or another failure mechanism is not yet selected.

Later terrain edits may affect vegetation already placed, including nearby trees the structure did not explicitly target. We need a policy for revalidation, removal, adaptation, or rejection, plus updates to affected geometry and collision. Re-running whole earlier generation passes could undo intentional later changes, so any repair/revalidation mechanism needs a defined scope.

Scope resolution and execution order are different concerns: an inherited forest rule can be authored on an ancestor but evaluated after descendant terrain modifiers. Local suppression can determine which inherited decorators participate before placement.

The phase list, numeric priority direction, allowed reordering, dependency validation, and stable tie-breaking remain undecided. The same inputs must produce the same results regardless of actual task scheduling or exploration order. Priority changes intentionally change generation inputs.

## Example authoring structure

Names below describe roles, not approved C++ classes. Decorators are listed together with containers to show attachment and scope, not execution order.

```text
World container
├── Hills decorator
├── Forest decorator
├── Clearing container
│   ├── Suppress inherited forest
│   └── Flatten terrain decorator
└── Birch grove container
    └── Modify inherited forest: birch trees
```

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
