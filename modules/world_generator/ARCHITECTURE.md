# World Generator Architecture Draft

Status: discussion draft, not approved for implementation. Read `VISION.md` for the requirements and examples. Class names, Godot base classes, terrain storage, and prototype scope remain undecided.

## Proposed generation stages

The user proposes separating child sizing/shaping/placement, terrain formation, and detail generation. The following responsibilities and outputs are an assistant interpretation for discussion.

| Stage | Responsibility | Conceptual output |
| --- | --- | --- |
| Region layout | Select child definitions, assign sizes and positions, and obtain child-defined region shapes within their constraints. | Region instances, spatial relationships, and generation enclosures. |
| Terrain formation | Produce the actual landscape inside those regions: surfaces, hills, caves, underground spaces, and terrain materials. | A queryable terrain description; representation not yet chosen. |
| Details | Place vegetation, constructed structures, and other content; allow local terrain adjustments where required. | Object placements and any terrain modifications they require. |

Region shape and terrain shape are distinct. A continent's outline and assigned vertical enclosure describe where it may generate; they do not fully describe its mountains, caves, or internal surfaces. Both stages must preserve the child's control of its own shape within the parent's constraints.

Details are not restricted to visual decoration. A generated structure could require a small foundation adjustment. How such edits are combined with terrain and other placements remains open. Rivers are not assigned to a stage yet.

## Definitions, instances, and stages are different distinctions

The earlier proposal distinguishes reusable authoring definitions, particular generated region instances, and the materialized world. The stage proposal describes the work performed using those objects. These ideas can coexist; they do not require choosing one model over the other.

For example, an Ocean definition could supply configurable layout, terrain, and detail behaviors. One generated ocean would apply those behaviors in its assigned space. Meshes, collision, and scene objects would represent the currently required part of the result. This is an example, not a selected class design.

**Assistant proposal:** keep layout, terrain, and detail behaviors independently replaceable within a shared authoring hierarchy. We have not agreed whether they are components attached to each definition, separate nodes, resources, or another arrangement.

## Terrain representation remains open

A conventional heightmap gives one height for each horizontal position. By itself, it cannot represent caves, overhangs, or multiple vertically stacked surfaces. It can still contribute an intermediate surface profile to a richer representation. The required result must support the three-dimensional world, but neither a particular volumetric format nor a meshing algorithm has been selected.

Reference: [Godot heightmap limitations](https://docs.godotengine.org/en/stable/classes/class_heightmapshape3d.html).

## Dependencies and integration still to design

- Every stage must preserve the agreed same-input/same-output behavior. Seed handling, stage inputs, and ordering need explicit contracts.
- Detail-generated terrain edits must be reflected consistently in terrain queries, visible geometry, collision, and persistence. Their application order and dependencies remain open.
- Rendering, collision construction, streaming, level of detail, saved changes, and optional simulation still need responsibility boundaries. The three generation stages do not define the whole runtime architecture.
- Child minimum-size failures, region transitions, layer ordering, and bounded generation still need decisions.

The next discussion is how a world author configures and replaces the three kinds of behavior while keeping the intended definition tree understandable.
