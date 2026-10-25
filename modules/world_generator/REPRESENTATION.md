# World Representation Discussion

Status: joint representation discussion, not approved for implementation. Separate terrain and other-object representations, chunks, and multiple LODs are confirmed. The signed scalar terrain field remains a candidate; storage, meshing, object cutting, and resolution are not chosen.

## Confirmed representation requirements

- Terrain and other objects may use separate representations.
- The world uses chunks and multiple levels of detail.
- A tree must permit cuts at arbitrary locations rather than only predetermined break points. This does not yet specify cut geometry, numerical precision, detachment rules, or the object representation.

The remaining question about voxel terrain is its appearance. The candidate stores scalar samples and extracts the intervening surface, rather than drawing a cube for each occupied sample. A sampled grid therefore need not create Minecraft-style steps. Coarse sampling can still produce faceting, lose thin features, and change silhouettes; smooth shading cannot recover absent geometry.

## Candidate: sparse chunked scalar terrain with materials

Represent terrain through a signed scalar field sampled in three dimensions, plus material data. Negative field values mean solid, positive mean air, and zero locates the surface. Distance-like values allow interpolation when extracting a smooth mesh. Arbitrary edits need not maintain an exact distance-to-nearest-surface field, so the field's guarantees must be specified rather than assumed.

For the agreed unmodified base, the field can be evaluated as `f(x, y, z) = y`. Negative Y is solid and positive Y is air. The initial material below the surface remains configurable design work.

This is a voxel representation without requiring visible cube geometry. Smoothness does not remove the sampling limit: thin features and narrow openings can disappear if the samples are too far apart. Sample spacing and quantization must be chosen against real editable-detail requirements.

Reference: [Voxel Tools smooth terrain and signed fields](https://voxel-tools.readthedocs.io/en/latest/smooth_terrain/). This is evidence of the technique, not a decision to use Voxel Tools or its mesher.

## Sparse storage and generation

**Assistant proposal:** address the world through integer chunk coordinates and use compact sample arrays in resident mixed chunks. Represent known uniform/default regions compactly and evaluate unmodified procedural areas when needed. Retain persistent edit data separately from disposable generated caches. Coarser sampled data and meshes can support distant detail levels.

Do not allocate a dense array for an endless world. Full-resolution residency, cache lifetime, edited-region storage, chunk boundaries, material variation, and LOD transitions all need explicit designs. An octree, spatial hash, and sparse-brick hierarchy are candidates with different costs, not interchangeable guarantees of best performance.

[OpenVDB's overview](https://www.openvdb.org/documentation/doxygen/overview.html) demonstrates sparse volumetric storage and separation of index space from world space. No OpenVDB dependency is selected.

## Concrete memory illustration

An illustrative uncompressed payload with a 16-bit signed field and a 16-bit material ID uses four bytes per sample. A `32 × 32 × 32` sample array is therefore `131072` bytes, or **128 KiB**. This excludes boundary samples, metadata, blending channels, meshes, collision, caches, and other overhead. Quantization range, material encoding, and chunk dimensions are not selected.

At fixed world volume, halving sample spacing in each axis produces eight times as many dense samples. At 25 cm spacing, centimeter-thick editable details cannot be reliably represented merely by drawing a smoother mesh. A fine grid used for every leaf, twig, and wall detail would need evaluation against memory and generation budgets.

## One volume for everything versus a hybrid

Using the same volumetric technique for ground, tree trunks, and walls could unify some material queries and destructive edits. However, geometry alone does not describe a tree's identity, a door hinge, shared-wall ownership, construction rules, or a detached log's motion. These need additional state even if their geometry is voxelized.

**Confirmed direction:** terrain and other objects are separate. **Assistant recommendation:** evaluate the scalar volume for terrain, while allowing world objects to retain independent identity, geometry, and state. Repeated foliage can use instanced meshes; objects needing local volumetric damage could eventually own their own volumes. The specific object representation is not selected.

Godot provides batched mesh instancing through [MultiMesh](https://docs.godotengine.org/en/stable/tutorials/performance/using_multimesh.html); spatial grouping and culling still matter. This does not solve tree destruction or structure simulation by itself.

A hybrid requires coordination between terrain and objects, but a single world query/edit interface could hide much of that complexity from decorators. Exact query semantics, object-volume interactions, and physics behavior remain to design. A query for terrain solidity must not silently substitute for a query for all objects occupying a point.

## Decisions and evidence still needed

- Smallest editable terrain feature and numerical precision/detail for arbitrary tree cuts.
- Target machines, view/edit distances, memory budget, and acceptable update latency.
- Object cutting representation and its interaction with identity, detached pieces, collision, and persistence; separate from the terrain grid's resolution.
- Representation, chunking, quantization, meshing, collision, edits, and LOD boundaries, evaluated together.
- Compare existing C++ backends and relevant algorithms before deciding what to implement ourselves.

Use representative cases to compare memory and generation/remesh latency rather than claim one representation is universally most efficient. Any benchmark/prototype implementation remains gated on joint architecture agreement.
