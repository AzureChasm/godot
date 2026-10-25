# World Representation Discussion

Status: the user has accepted the high-level terrain direction: sparse signed scalar field plus material data, separate objects, chunks, and multiple LODs. Full architecture is not yet approved for implementation. Concrete storage, meshing, object cutting, and resolution remain open.

## Confirmed representation requirements

- Terrain and other objects may use separate representations.
- The world uses chunks and multiple levels of detail.
- A tree must permit cuts at arbitrary locations rather than only predetermined break points. This does not yet specify cut geometry, numerical precision, detachment rules, or the object representation.
- Initial editable terrain detail is roughly fist-sized. This is a feature-size target, not an agreed voxel spacing or exact measurement.
- Efficiency and scalability matter even though the user's current hardware is powerful. No minimum target machine, frame-rate, memory, or latency budget is set yet.
- Ordinary digging is gradual. Earth magic must support both an explosion-like change that appears essentially instant and a large transformation unfolding over several seconds. Example dimensions and durations from the discussion are illustrative, not fixed budgets. Maximum affected volume, simultaneity, and acceptable preparation delay remain open.

## Current resolution and editing discussion

**Assistant proposal:** evaluate candidate fine sample spacings against fist-sized positive and negative features, including features between sample points and near chunk/LOD boundaries. A provisional comparison of 5 cm and 10 cm spacing could expose the cost/detail tradeoff; neither value is selected or guaranteed to preserve every fist-sized feature. Smooth interpolation alone does not establish an adequate minimum feature size.

Separate two edit workloads: repeated small digging operations and large earth-magic operations spanning many chunks. Candidate scheduling strategies include bounded background work, merging overlapping dirty-chunk requests, skipping obsolete rebuilds, and prioritizing nearby collision/visible results. Their ownership and synchronization contracts are not designed.

A completed operation must still leave a consistent world. Background computation must not accidentally expose contradictory terrain and collision. Both instant-looking and progressive spell behavior are confirmed requirements.

**Assistant proposal:** an instant edit prepares its affected data and required presentation/physics changes before publishing a coherent result. A progressive transformation consists of intentional steps, each leaving a valid world. These could share the same edit-request and validation concepts. Preparation latency, chunk coordination, conflicts with simultaneous edits, cancellation, and publication mechanisms are not yet designed. Instant appearance does not establish an unlimited zero-cost edit budget.

The user clarifies that spell magnitude depends on the wizard; no fixed house/street/hillside maximum is specified. Wizard levels and spell strength belong to game rules, while the reusable engine receives the resulting edit parameters and affected space. Do not require a single gameplay maximum to continue architecture discussion.

**Assistant proposal:** separate variable edit magnitude from configurable execution budgets. An edit description expresses desired shape, material changes, and instant/progressive presentation intent; a scheduler bounds concurrent work and coordinates affected chunks. Small and large edits share the same contract without promising equal latency. Handling overload through preparation delay, rejection, cancellation, or a different presentation policy remains undecided; do not silently change an instant request into a progressive spell.

Scalability controls should distinguish visual quality and residency budgets from authoritative world resolution. Lowering a visual setting must not silently change terrain edits or remove a traversable feature from the underlying world. The exact LOD/collision policies remain open.

The remaining question about voxel terrain is its appearance. The candidate stores scalar samples and extracts the intervening surface, rather than drawing a cube for each occupied sample. A sampled grid therefore need not create Minecraft-style steps. Coarse sampling can still produce faceting, lose thin features, and change silhouettes; smooth shading cannot recover absent geometry.

## Agreed direction: sparse chunked scalar terrain with materials

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

- Convert the agreed fist-sized terrain feature target into validated sample spacing/precision; define numerical precision/detail for arbitrary tree cuts separately.
- Target machines, view/edit distances, memory budget, and acceptable update latency.
- Object cutting representation and its interaction with identity, detached pieces, collision, and persistence; separate from the terrain grid's resolution.
- Representation, chunking, quantization, meshing, collision, edits, and LOD boundaries, evaluated together.
- Compare existing C++ backends and relevant algorithms before deciding what to implement ourselves.

Use representative cases to compare memory and generation/remesh latency rather than claim one representation is universally most efficient. Any benchmark/prototype implementation remains gated on joint architecture agreement.
