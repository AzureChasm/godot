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

## Generation order

**Confirmed direction:** decorators need an ordering mechanism; terrain work precedes dependent structure and fauna generation. The user suggests priorities. The precise phase system and priority semantics are not agreed.

**Assistant proposal:** use broad generation phases, plus priority or explicit ordering within a phase. Terrain formation would precede content that queries the resulting terrain. Possible further phases include structures, vegetation, and fauna, but their ordering is not yet selected.

Scope resolution and execution order are different concerns: a forest rule can be authored on an ancestor but evaluated after descendant terrain modifiers. Likewise, a local suppression rule can affect which inherited decorators participate before any relevant placement occurs.

A structure may need to choose a site, prepare a foundation, and then place its objects. This may require several operations or dependencies rather than one priority number on one indivisible action. How decorators express these requirements remains open.

Within terrain work, raising hills and flattening a clearing can produce different results depending on order. We must define intentional composition, stable tie-breaking, and the handling of incompatible dependencies. Loading order must not determine output.

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
- Structure planning and terrain preparation without circular dependencies.
- Container boundaries, falloff, biome transitions, size constraints, and bounded generation.
- Terrain queries, rendering, collision, streaming, level of detail, persistent changes, and optional simulation boundaries.
- C++/Godot types, lifecycle, and prototype success criteria, followed by joint architecture agreement before implementation.
