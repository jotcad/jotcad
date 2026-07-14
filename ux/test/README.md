# UX Frontend Unit & Rendering Tests

This directory contains unit tests, state synchronization verifications, and HTML snapshot tests for the SolidJS and WebGL rendering components.

## Test Suites

* `blackboard_controls.test.js` — Verifies blackboard formula tracking, parameter registry modification, and canvas redraw trigger loops.
* `jot_translation.test.js` — Verifies correct serialization and translation between native JS objects, JCB binary payloads, and safe Base64 carrier strings.
* `sync.test.js` — Verifies state synchronization loops between the local UI blackboard cache and the VFS layer.
* `smoke.test.jsx` / `terminals.test.jsx` — SolidJS component mount and reactive update assertions.

## Rendering Snapshot Baselines

* `*_render.html` / `*_snapshot.png` — Statically exported HTML mesh representations and corresponding visual baselines (e.g. `hexagon`, `triangle`, `offset`, `outline`) used to assert rendering layout consistency and check against regression.
