# Pour Orientation, Air Trap Detection, & Vent Synthesizer

This directory implements automated gravity pour orientation optimization, topological Morse air-trap/peak detection, and parametric sprue/riser geometry generation for slipcasting.

## Modular Component Index

| Header File | Responsibility |
| :--- | :--- |
| [`types.h`](./types.h) | Pure data models (`PourParams`, `PeakCluster`, `PourResult`). |
| [`orientation.h`](./orientation.h) | $\mathbb{S}^2$ spherical search optimizing for minimal peak count and steep ceiling slopes ($\phi \ge 15^\circ$). |
| [`traps.h`](./traps.h) | 1-ring topological local height peak detection and BFS island clustering. |
| [`vents.h`](./vents.h) | Parametric primary pour funnel (sprue) and secondary air riser synthesis via exact boolean union. |

## Pipeline Flow
1. **Input Mesh** $\to$ `find_optimal_pour_orientation()` aligns the model with gravity $+Z$.
2. **Oriented Mesh** $\to$ `detect_peaks_and_air_traps()` extracts Primary Pour Gate and Secondary Vent Summits.
3. **Synthesis** $\to$ `synthesize_vents_and_sprue()` fuses the pour funnel and riser vents into a single solid ready for `jot/mold` multi-piece decomposition.
