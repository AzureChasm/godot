# World Generator Development

- Read `VISION.md`; confirmed vision is separate from proposals. Module name is provisional.
- Read `ARCHITECTURE.md` and `PROTOTYPE_PLAN.md`. User approved the current design for the first prototype; keep remaining long-term proposals distinct from implemented behavior.
- Read `REPRESENTATION.md` and the dated handoff in `ARCHITECTURE.md`. High-level sparse signed-field terrain plus materials is accepted, along with separate objects, chunks, multiple LODs, and arbitrary tree cut locations. Prototype defaults and provisional backend are documented in `README.md`; long-term storage, meshing, object cutting, sample spacing, and budgets remain open.
- Keep module code reusable and independent of Astray content and paths. C++ is the default.
- Authoring tree contains containers only; decorators are list-property entries, not child/attachment nodes. Root area present means bounded generation; absent means unbounded. Resource-backed decorator references are the proposed Godot mapping; don't mutate shared ancestor config to implement local overrides.
- Current focus is terrain architecture and a terrain-first milestone. City/building generation is deferred; preserve its recorded ideas without expanding that design unless requested.
- Review feedback: automatic generation is runtime-only, not on opening an editor scene. Explicit manual previews are allowed. Preserve user-edited demo settings. Meshing topology must use the sampled field gradient, not analytic shading normals; retain the temporary crater-topology regressions when adding the test suite later.
- Architecture is jointly designed with the user. Explain responsibilities, data flow, tradeoffs, and C++ clearly enough for user review. Critically evaluate suggestions; do not rubber-stamp them.
- User knows C++; Godot internals/module development are new. Explain Godot-specific conventions at first use; avoid basic C++ tutorials unless requested.
- User approved the current architecture and explicitly authorized the first prototype. Implement the narrow `PROTOTYPE_PLAN.md` slice on `feature/world-generator-prototype`; document provisional backend choices. Material design departures still return to discussion.
- Before implementing, create a new feature branch. User reviews C++ and behavior in the Godot editor; make initial adjustments before the prototype's first implementation commit. Preserve existing history.
- Establish module automated test structure after the initial reviewed prototype commit; still build and manually validate the prototype.
- Iterate in small editor-reviewable improvements; support the user making their own changes. Revisit material architecture changes together.
- Do not push the feature branch without explicit authorization.
