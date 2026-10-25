# World Generator Architecture Draft

Status: joint design in progress; not approved for implementation. Read `VISION.md` for requirements. The authoring topology below follows the user's direction. Concrete classes, Godot base classes, data contracts, terrain storage, and prototype scope are undecided.

## Separate authoring roles

### Region layouters

The node originally called World Generator is a region layouter. Ocean and Continent can also be region layouters. Each selects, sizes, shapes, and places its children within assigned space. Layouters can contain further layouters recursively; no fixed authoring depth is assumed. Runtime termination and validation rules remain to design.

Region shape and terrain shape are distinct. A continent's outline and vertical enclosure describe where it can generate; they do not specify its internal mountains, caves, or surfaces. A child retains control of its region shape within the parent's constraints.

### Terminal terrain formations

Each terminal region has one terrain formation that produces its actual three-dimensional landscape. Different branches of one layout tree can end in different formations. This describes one formation entry point per terminal region, not one formation for the entire world.

How a terrain formation internally composes hills, caves, or other operations remains open. A single formation entry point does not yet prescribe a single indivisible algorithm.

### Inherited details

Details are a separate role that can be attached at any level of the tree. They apply throughout that node's descendant regions. A forest detail rule can therefore cover several nested regions with different terrain formations, yielding one forest across hills, plains, and other terrain.

Authoring scope differs from execution order: a forest rule can be authored on a high-level region while placing its trees using terrain generated farther down the hierarchy. Detail passes may also make local terrain adjustments. How inherited rules combine, override, or exclude one another remains open.

## Example authoring structure

The following notation illustrates responsibilities and scope, not final Godot class names or required scene-node relationships:

```text
Region Layouter: Forest area
├── Details: Forest vegetation [applies across descendant regions]
├── Region Layouter: Rolling country
│   └── Terrain Formation: Hills
└── Region Layouter: Open country
    └── Terrain Formation: Plains
```

Forest vegetation is shared in scope while the two terminal regions have different formations. More nested layouters can be inserted before either terminal formation. The exact representation of attached detail rules is still undecided.

The earlier assistant suggestion of giving every biome a fixed layout/terrain/detail component bundle is superseded by these separate roles.

## Definitions, generated regions, and visible output

These roles describe authored generation behavior. Reusable definitions, particular generated instances, and the materialized world remain distinct conceptual concerns; their concrete representation is not agreed.

One Ocean layouter definition can describe multiple generated ocean regions. Meshes, collision, and placed objects represent the currently needed parts of the result. How much region state is retained, regenerated, or saved remains open.

## Terrain representation remains open

A conventional heightmap gives one height for each horizontal position. By itself, it cannot represent caves, overhangs, or multiple vertically stacked surfaces. It can still contribute an intermediate surface profile to a richer representation. Neither a particular volumetric format nor a meshing algorithm is selected.

Reference: [Godot heightmap limitations](https://docs.godotengine.org/en/stable/classes/class_heightmapshape3d.html).

## Dependencies and remaining decisions

- Define inputs and outputs for layouters, formations, and details, preserving same-input/same-output behavior and independence from exploration order.
- Define detail inheritance, local overrides or exclusions, rule identity, and evaluation scope. Ancestor rules must not accidentally multiply placement density merely because the tree has more levels.
- Determine when details query or alter terrain, and how changes reach terrain queries, geometry, collision, and persistence.
- Place transition responsibility and reconcile it with recursive layout and terminal formations.
- Decide child minimum-size failure behavior, layer ordering, and bounded generation.
- Define rendering, collision construction, streaming, level of detail, saved changes, and optional simulation boundaries.
- Agree on C++/Godot types, lifecycle, and the first prototype's visible success criteria before implementation.
