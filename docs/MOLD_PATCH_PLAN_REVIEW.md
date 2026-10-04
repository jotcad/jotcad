# Review: Patch-First Candidate Plan (§11–12 of MOLD_DRAW_DIRECTION_OPTIMIZER_DESIGN.md)

**Status:** Open review — corrections not yet folded into the design doc  
**Reviewed:** [`MOLD_DRAW_DIRECTION_OPTIMIZER_DESIGN.md`](./MOLD_DRAW_DIRECTION_OPTIMIZER_DESIGN.md) §11 (Vector vs. Patch Duality) and §12 (Implementation Plan)  
**Evidence:** Code in `geo/ops/mold/` and the 2-Way Planar Cross run (snapshot 569, timed out at 1200 s)

The overall direction (branch on distinct patches, not on continuous vectors) holds up. The issues below must be resolved before or during implementation.

---

## A. Correctness issues (fix first)

### A1. The envelope cache can return the wrong envelope
In `get_raw_stock` ([`beam_search.h`](../geo/ops/mold/beam_search.h)), `envelope_cache` is keyed by `VectorKey{dir}` only. But `compute_exact_upper_envelope_mesh` also depends on:
- `is_handled_map` (comes from the parent node), and
- `node->tentative_patch_faces`.

If the same direction comes up under a different parent, it gets a stale envelope. Mean-shift (Phase 2) would make this *more* likely, because it funnels many candidates onto the same few vectors.

**Fix:** include parent identity (or a hash of the parent's handled set) in the cache key, before Phase 2.

### A2. "Residual stock subtraction failed" is unexplained
Iterations #3–#8 each failed with this message, at roughly 20 s each. Deduplication would hide the symptom, but it is a geometry failure (`corefine_difference` returned false, or produced a non-closed mesh). Under the rule against swallowing geometric failures, it needs its own root-cause diagnosis.

---

## B. Places the plan conflicts with itself

### B1. Two different definitions of the "best" vector
- §11.2 defines `d*` as the vector that maximizes the worst-case draft, `argmax_d min_f (n_f · d)`. That is the centre of the smallest cone enclosing the normals.
- §11.4 computes `d*` as the area-weighted normal centroid.

These are different points. The doc must either pick one or state plainly that the centroid is a cheap stand-in.

### B2. Mean-shift breaks zero-draft sliding walls
With `draft = 0`, walls with `n·d = 0` belong to the patch. On an asymmetric part their normals pull the centroid sideways, and the next iteration then drops those walls. That is exactly the 1D sliding case §10.2 protects.

Exempting only the cross-product (sliding-axis) candidates is not enough, because a face-normal candidate also has walls lying exactly at `n·d = 0`.

**Fix:** faces at `n·d = 0` stay in the patch but do not contribute to the centroid sum.

### B3. The Jaccard tie-breaker decides nothing at zero draft
At `draft = 0`, almost every patch has a worst-case margin of exactly 0, so "keep the vector with the best worst-case margin" picks nothing. The rule also does not say which patch the margin is measured over (P_A, P_B, or their union). In addition:
- 0.85 is an arbitrary threshold;
- greedy clustering depends on the order candidates arrive in.

**Fix:** keep the candidate with the most new (virgin) area, and break ties on margin.

### B4. The dedup acts on the pre-screen patch, not the carved result
`extract_candidate_patch` ignores occlusion. The trace shows how far off it can be: iteration #2 was predicted at 623 mm², but after carving, 1405 mm² was still unhandled. Two candidates that look 85% alike before carving can produce quite different envelopes.

This does not kill the idea, but the doc must label patch dedup a **heuristic**, not an exact equivalence.

---

## C. Project-rule violations

### C1. File size (atomic file rule, ~300 lines)
`beam_search.h` is already 763 lines. Phase 1 should go in a new file (e.g. `geo/ops/mold/patch_dedup.h`) and mean-shift in `geo/ops/mold/mean_shift.h`, with `geo/ops/mold/README.md` updated. Ideally `beam_search.h` gets split as well.

### C2. Exact arithmetic (`EK::FT` mandate)
The stored `face_normals` are unnormalized cross products, which are already proportional to area times the unit normal. So the centroid `Σ face_normals[f]` can be summed in exact arithmetic with no `sqrt`.

Normalization should happen **once per iteration**, at the boundary, as with other candidate vectors. Without that, exact rationals grow in size with each iteration.

### C3. Phase 3 is in the wrong layer
The duplicate search (a second "Starting Level-by-Level Priority Search" while the first was still running at about 22 CPU-minutes) means the VFS's `PENDING` coalescing did not apply. That falls under [`VFS_SPECIFICATION.md`](./VFS_SPECIFICATION.md), which must be read before touching it.

An ad-hoc in-flight map in `mold_op.h` would bypass the VFS contract. Also, abandoned searches are never cancelled, which is a separate gap.

---

## D. Unrealistic targets

### D1. "Under 120 s" is a guess
Even a perfect 4-piece path costs about 4 × (20–45 s corefine + 15–58 s kiss resolution) ≈ 3–6 minutes. Dedup removes the *wasted* branches but does not make each cut cheaper.

An honest first target is **under ~10 minutes**, with per-cut cost (corefine and kiss resolution on growing residual stock) as the next thing to work on.

### D2. No fast test
Every check in the plan is a 20-minute end-to-end run. Needed:
- A fast, log-based check for Phase 1: `[Dedup] N → k` at level 0, expecting k ≈ 4–8 on the planar cross.
- A tool that actually measures the 0.000 mm³ dead-volume criterion.

---

## E. Proposed revised order

| Step | Change | Risk |
| :--- | :--- | :--- |
| 0 | Fix the envelope cache key (A1) and add `[Dedup] N → k` logging | Low |
| 1 | Diagnose "residual stock subtraction failed" (A2) | — |
| 2 | Patch-signature dedup in a new file: exact face-set equality first, then Jaccard; tie-break on new area, then margin (B3, B4, C1) | Low |
| 3 | Mean-shift in a new file: exact sum, one normalization per iteration, faces at `n·d = 0` kept but excluded from the sum; settle centroid vs. best-margin (B1, B2, C2) | Medium |
| 4 | Read the VFS spec, then find out why `PENDING` coalescing failed (C3) | Medium |
| 5 | Benchmarks with realistic targets and a fast dedup test (D1, D2) | — |
