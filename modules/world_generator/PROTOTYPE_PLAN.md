# First Terrain Prototype Plan

Status: user approved the current architecture and authorized implementation of this first prototype. Engine branch: `feature/world-generator-prototype`. The user reviewed the prototype and initial adjustments and authorized the initial implementation commit.

## What the user should be able to try

Create a root region container in the Godot editor, add nested container children, and assign decorators through list properties in the Inspector. Generate a small smooth terrain preview. Change a decorator, regenerate, and inspect a hill, a carved cavity, and a floating solid. The preview must demonstrate three-dimensional geometry rather than block faces.

An explicit root area limits generation. An unbounded root remains unbounded in definition; the preview view requests only a finite area, so this demo does not attempt to generate an infinite domain eagerly.

## Proposed prototype responsibilities

- `RegionContainer`: a scene-tree authoring node with child containers, an explicit bounded/unbounded root setting, prototype box enclosures, and a decorator resource-list property.
- `WorldDecorator`: an abstract reusable configuration/behavior contract. Initial implementations add and subtract sphere-shaped terrain; parameter overrides must not mutate shared ancestor resources.
- Terrain model: signed-field/material samples in addressed chunks. Proposed debug-friendly settings: 32 cells per chunk edge, 10 cm sample spacing, 32-bit field values, and a basic material ID. Include the border samples required by meshing. These are experimental settings, not guarantees about minimum feature size or a permanent world format.
- Edit/generation coordinator: prepare candidate chunk data, validate it, and retain the previous preview on failure. Start synchronously for a bounded preview; no claim of production multithreaded publication.
- Mesher and view: separate surface extraction from scene presentation. Proposed first backend is marching tetrahedra with static triangle collision from the resulting surface, regenerated synchronously for the bounded editor preview. This is a provisional, replaceable smooth mesher, not a selected production LOD-transition algorithm. Present a clearly lit, inspectable preview.

## Scope boundaries

This slice demonstrates the agreed authoring distinction, inherited decoration, bounded/on-demand preview, three-dimensional terrain, chunk-backed data, and replacement publication. It does not implement endless runtime streaming, multiple visible LOD transitions, persistent saves, earth-magic performance guarantees, biome transitions, full cascade/antidecorator behavior, or object cutting. Those requirements remain in the vision.

The long-term consistency contract remains required; this prototype must honestly document which data/render synchronization it demonstrates. The proposed starting preview is an 8 m cube crossing `Y = 0`, configurable in the editor. Root-area edge behavior is limited to generating requested surfaces inside the preview, not building arbitrary artificial boundary walls. Review the proposed defaults, collision, and provisional mesher before implementation.

## Review and validation

Build the custom editor, perform an appropriate module smoke check, and review the result in Godot. Explain the C++ structure and Godot registration/property bindings. The user reviews and requests initial adjustments before the first implementation commit. Updated review agreement: defer a dedicated unit-test suite for now; use the demo, user inspection, builds, and targeted smoke/regression checks. No branch pushes are authorized.

## Working setup

Open `astray-engine.code-workspace` in VS Code for C++ navigation, building, and debugging. Codex can work from the current conversation or the installed IDE extension against the same checkout. Use the built Godot editor for visual review. The reusable module and its demo must not depend on Astray game content.
