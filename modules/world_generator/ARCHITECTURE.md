# World Generator Architecture Draft

Status: joint design in progress; not approved for implementation. Read `VISION.md` for requirements. The authoring topology below follows the user's direction. Concrete classes, Godot base classes, data contracts, terrain storage, and prototype scope are undecided.

## Current alternative under discussion: containers and decorators

The user proposes two public authoring concepts: containers structure and position regions; decorators supply modifications. Generation starts from a simple, otherwise empty flat surface. This is an alternative under evaluation, not full architecture approval.

Under this proposal, hills, caves, terrain materials, vegetation, and structures can all be decorators with different effects. A separate terminal Terrain Formation node would no longer be required. The earlier three-role model below is retained for comparison while this choice is discussed.

**Assistant assessment:** this can simplify authoring and allow reuse across terrain and detail work. Internally, terrain modification and object placement still need explicit contracts, inputs, and dependencies. A tree placer may query the surface after terrain modifiers have acted; a structure may also request a foundation edit. A common decorator concept does not yet imply one undifferentiated execution method or a fixed pass order.

Containers retain spatial layout responsibilities. Decorators change world content within their effective scope. Whether a decorator may also change a container's boundary, and how that would interact with child allocation, remains open.

**Assistant proposal for the base:** represent ground below a configurable flat elevation and empty space above it, with no vegetation or structures initially. This would supply a three-dimensional starting solid that can be carved or extended. The user has not yet clarified whether "flat surface" means this volume or a different starting condition. It does not choose the terrain storage or meshing method. Other base conditions could be considered later for reusable bounded generation.

The cascading decoration intent still applies: inherited rules can be locally modified, replaced, or suppressed. However, suppressing a terrain modification raises new questions: does a local region inherit the base terrain instead, remove only one modifier, or apply another transformation? Resolving rule inheritance before evaluation is an assistant proposal, not an agreed implementation.

Remaining decisions include the base's meaning, decorator capabilities and dependencies, composition order, spatial falloff at boundaries, and suppression semantics. If this alternative is adopted, the earlier rule requiring one terminal formation per final region must be explicitly superseded.

## Earlier three-role model for comparison

### Region layouters

The node originally called World Generator is a region layouter. Ocean and Continent can also be region layouters. Each selects, sizes, shapes, and places its children within assigned space. Layouters can contain further layouters recursively; no fixed authoring depth is assumed. Runtime termination and validation rules remain to design.

Region shape and terrain shape are distinct. A continent's outline and vertical enclosure describe where it can generate; they do not specify its internal mountains, caves, or surfaces. A child retains control of its region shape within the parent's constraints.

### Terminal terrain formations

Each terminal region has one terrain formation that produces its actual three-dimensional landscape. Different branches of one layout tree can end in different formations. This describes one formation entry point per terminal region, not one formation for the entire world.

How a terrain formation internally composes hills, caves, or other operations remains open. A single formation entry point does not yet prescribe a single indivisible algorithm.

### Inherited details

Details are a separate role that can be attached at any level of the tree. They apply throughout that node's descendant regions. A forest detail rule can therefore cover several nested regions with different terrain formations, yielding one forest across hills, plains, and other terrain.

Authoring scope differs from execution order: a forest rule can be authored on a high-level region while placing its trees using terrain generated farther down the hierarchy. Detail passes may also make local terrain adjustments.

### Containers and cascading decorations

The user wants regions to behave like nested HTML-style containers: decorations on a container affect its descendants, and descendants can also declare their own decorations. A forest island can contain different terrain formations while sharing forest decoration. A clearing suppresses inherited forest decoration; a birch forest modifies or replaces it locally.

This is scoped inheritance through the region tree, not necessarily C++ class inheritance. The analogy does not commit us to a browser, CSS parser, selector language, or a particular Godot node representation.

The user's suggested name for a suppression rule is an **antidecorator**. The behavior is part of the intended direction; whether it becomes a separate type, an operation on a decoration rule, or another representation remains open.

**Assistant interpretation for discussion:** distinguish adding a decoration, modifying selected settings of an inherited decoration, replacing an inherited decoration, and suppressing a targeted decoration. For a birch forest, changing tree species need not discard unrelated inherited settings. Whether clearing suppression targets a whole forest package or only tree placement needs definition.

**Assistant proposal:** resolve the effective decoration rules for a target region before placing content, so suppression prevents generation rather than generating objects only to delete them. Local rules affect their own subtree, leaving siblings unchanged. Runtime edits to existing objects are a separate concern; suppression must not silently erase saved player modifications.

Rule identity, selection by category, precedence, and the ability of deeper descendants to restore a suppressed decoration are undecided. A candidate default is that the nearest explicit setting wins, but this is not approved.

## Example of the earlier three-role authoring structure

The following notation illustrates responsibilities and scope, not final Godot class names or required scene-node relationships:

```text
Region Layouter: Forest area
├── Details: Forest vegetation [applies across descendant regions]
├── Region Layouter: Rolling country
│   └── Terrain Formation: Hills
├── Region Layouter: Clearing
│   ├── Antidecorator: Suppress inherited forest
│   └── Terrain Formation: Plains
└── Region Layouter: Birch grove
    ├── Decoration override: Birch forest
    └── Terrain Formation: Hills
```

Forest vegetation is inherited by default, suppressed in the clearing, and changed in the birch grove. More nested layouters can be inserted before any terminal formation. Decoration and antidecorator names in the diagram describe authoring roles, not approved C++ classes.

The earlier assistant suggestion of giving every biome a fixed layout/terrain/detail component bundle is superseded by these separate roles.

## Definitions, generated regions, and visible output

These roles describe authored generation behavior. Reusable definitions, particular generated instances, and the materialized world remain distinct conceptual concerns; their concrete representation is not agreed.

One Ocean layouter definition can describe multiple generated ocean regions. Meshes, collision, and placed objects represent the currently needed parts of the result. How much region state is retained, regenerated, or saved remains open.

## Terrain representation remains open

A conventional heightmap gives one height for each horizontal position. By itself, it cannot represent caves, overhangs, or multiple vertically stacked surfaces. It can still contribute an intermediate surface profile to a richer representation. Neither a particular volumetric format nor a meshing algorithm is selected.

Reference: [Godot heightmap limitations](https://docs.godotengine.org/en/stable/classes/class_heightmapshape3d.html).

## Dependencies and remaining decisions

- Define inputs and outputs for layouters, formations, and details, preserving same-input/same-output behavior and independence from exploration order.
- Define decoration identities/targets, precedence, partial modifications versus replacement, and whether descendants can restore suppressed decorations. Ancestor rules must not accidentally multiply placement density merely because the tree has more levels.
- Determine when details query or alter terrain, and how changes reach terrain queries, geometry, collision, and persistence.
- Place transition responsibility and reconcile it with recursive layout and terminal formations.
- Decide child minimum-size failure behavior, layer ordering, and bounded generation.
- Define rendering, collision construction, streaming, level of detail, saved changes, and optional simulation boundaries.
- Agree on C++/Godot types, lifecycle, and the first prototype's visible success criteria before implementation.
