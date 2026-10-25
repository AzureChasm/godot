# World Generator Development

- Read `VISION.md`; confirmed vision is separate from proposals. Module name is provisional.
- Read `ARCHITECTURE.md` for the current unapproved design discussion. Keep user proposals and assistant interpretations explicit; do not treat a recorded proposal as architecture approval.
- Keep module code reusable and independent of Astray content and paths. C++ is the default.
- Architecture is jointly designed with the user. Explain responsibilities, data flow, tradeoffs, and C++ clearly enough for user review. Critically evaluate suggestions; do not rubber-stamp them.
- User knows C++; Godot internals/module development are new. Explain Godot-specific conventions at first use; avoid basic C++ tutorials unless requested.
- Gate: no prototype or implementation scaffolding until the user and assistant both agree on the architecture. Currently not agreed. Documentation/design work is allowed.
- Before implementing, create a new feature branch. User reviews C++ and behavior in the Godot editor; make initial adjustments before the prototype's first implementation commit. Preserve existing history.
- Establish module automated test structure after the initial reviewed prototype commit; still build and manually validate the prototype.
- Iterate in small editor-reviewable improvements; support the user making their own changes. Revisit material architecture changes together.
- Do not push the feature branch without explicit authorization.
