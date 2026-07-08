# River Simulator Specification

The JotCAD River Simulator is a physical, particle-based hydraulic and geomorphological model designed to simulate alluvial river meandering, sediment sorting, and bed armoring on a 2D heightmap grid.

## 1. Two-Fraction Sediment Model

To achieve realistic river geomorphology (self-regulating bed armoring and cohesive bank stabilization), the soil layer is divided into two separate grain-size fractions:

*   **Clay/Silt (Cohesive)**:
    *   **Behavior**: Represents cohesive bank material that is difficult to erode but remains in suspension once picked up (slow settling velocity).
    *   **Erosion Coefficient ($K_{\text{erode\_clay}}$)**: `0.0001f`
    *   **Deposition Coefficient ($K_{\text{dep\_clay}}$)**: `0.0001f`
*   **Sand/Gravel (Non-Cohesive)**:
    *   **Behavior**: Represents heavy bedload material that is easily eroded under high shear stress but settles very quickly when water slows down (fast settling velocity).
    *   **Erosion Coefficient ($K_{\text{erode\_gravel}}$)**: `0.0001f`
    *   **Deposition Coefficient ($K_{\text{dep\_gravel}}$)**: `0.0001f`

---

## 2. Shear Velocity & Gravel Mobility

Gravel transport capacity is regulated by a **critical shear velocity threshold**. Below this velocity, gravel is completely immobile and deposits on the bed, forming an armor layer:

*   **Immobility Threshold**: `speed <= 0.6f`
*   **Mobility Interpolation**:
    $$\text{gravel\_mobility} = \text{clamp}\left(\frac{\text{speed} - 0.6}{0.2}, 0.0, 1.0\right)$$
*   **Capacity Split**:
    $$C_{\text{clay}} = C \times 0.5$$
    $$C_{\text{gravel}} = C \times 0.5 \times \text{gravel\_mobility}$$

---

## 3. Numerical Boundary Conditions

To maintain simulation stability and prevent the spring head (water source) from eroding its own inlet and trapping water in a closed depression:

*   **Non-Erodible Boundaries**: Cells located along the grid boundaries ($x=0, x=w-1, y=0, y=h-1$) are marked non-erodible. Erosion and deposition calculations are skipped for these cells, keeping the inlet and outlet elevations perfectly stable.
*   **Constant Sediment Feed**: Water droplets spawn at the spring head saturated with clay and gravel to match the local carrying capacity ($\text{sediment} = C \times 0.8$), preventing severe headwater erosion.
*   **Dynamic Boundary Drainage**: When `DRAIN_MARGINS` is set to `1` in the soil profile's `WORLD` block, all surface water reaching the grid margins is destroyed. This prevents bathtub-style flooding in closed, flat basins.

---

## 4. Hydrological Modes (Rain vs. Spring)

The water delivery method is fully data-driven and configured in the `.soil` configuration file:

*   **Widespread Rainfall Mode**:
    *   **Trigger**: Configured when `SPRING_X` and `SPRING_Y` are negative (e.g., `-1`).
    *   **Behavior**: Spawns `NWATER` particles randomly across the entire grid, simulating widespread uniform rainfall.
*   **Point-Source Spring Mode**:
    *   **Trigger**: Configured with positive coordinates (e.g., `SPRING_X 128`, `SPRING_Y 128`).
    *   **Behavior**: Spawns all `NWATER` particles at the specified spring vent coordinate, simulating a high-volume point source.
    *   **Pressure Feedback on Spawning**: To prevent vertical water stacking, droplets spawn at the cell with the **lowest local hydraulic head** in a $5 \times 5$ neighborhood around the vent, mimicking water bubbling out of the path of least resistance.

---

## 5. Water Leveling & True Hydraulic Head

Water leveling is handled by a recursive cellular automata step (`cascade_water`):

*   **True Hydraulic Head**: Water moves downhill strictly according to the total hydraulic head:
    $$H = \text{bedrock} + \text{soil} + \text{water}$$
    Water is shifted to the lowest-head neighbor to level out local pressure differences.
*   **Configurable Leveling Depth**:
    *   The base leveling recursion depth is defined by `BASE_SPILL` in the `WORLD` block.
    *   Hilly profiles use a low baseline (`BASE_SPILL 3`) to prevent water from unnaturally flattening on slopes.
    *   Flat profiles use a high baseline (`BASE_SPILL 20`) to allow water to propagate horizontally over long distances and form flat lakes.
*   **Pressure Feedback on Deposition**:
    *   To prevent artificial vertical water spires at droplet termination points, droplets deposit water in the cell with the **lowest hydraulic head** in a $3 \times 3$ neighborhood around their final stopping position.

---

## 6. Deep Alluvial Topography

To ensure the river behaves as a true alluvial stream rather than incising down to tectonic bedrock:

*   **Deep Alluvial Valley**: The initial bedrock floor is set deep ($1.0\text{m}$), and the soil layer is set thick ($20.0\text{m}$), maintaining the identical overall surface topography but providing a massive soil buffer. The channel bed remains suspended high within the soil layer.

---

## 7. Visual Color Mapping

The clay and gravel fractions are blended in the image exporters (`terrain.png`, `terrain_iso.png`, and `terrain_side.png`) to provide visual feedback on sediment sorting:

*   **Clay Color**: Warm reddish-brown (`RGB: 130, 75, 55`)
*   **Gravel Color**: Light sandy-grey (`RGB: 190, 175, 155`)
*   **Grass Cover**: Green (`RGB: 75, 145, 60`), tinted organically by the underlying clay/gravel ratio.
*   **Active Channel**: Bare sediment color is exposed where water is present or flow accumulation is high ($F_{\text{acc}} > 12.0$), showing reddish-brown clay banks and a sandy-grey gravel riverbed.
*   **Standing Water**: Wet channels and lakes are colored dynamically from light cyan to deep navy blue based on water depth. Dry channels are rendered as sandy-brown riverbeds (`RGB: 180, 160, 130`).

---

## 8. Comparative Literature & Design References

The JotCAD River Simulator bridges physical principles from two main procedurally generated hydrological references:

### A. Nick McDonald (`weigert/SimpleHydrology`)
*   **Concept Borrowed**: Particle-based hydraulic erosion and deposition on a 2D heightmap. Droplets act as agents carrying sediment capacity determined by slope, speed, and volume.
*   **Refinement**: We split McDonald's single-layer soil erosion into two distinct material classes (clay and gravel) to model bank stability and bed armoring under bedrock constraints.

### B. Zoltan Sylvester (`zsylvester/meanderpy`)
*   **Concept Borrowed**: Upstream flow curvature lag. In nature, meanders translate downstream because helical secondary flow takes some distance to grow and strike the bank.
*   **Refinement**: While `meanderpy` uses a 1D centerline spline convolution model, we represent this physical lag in our 2D model by setting a high velocity `inertia` ($0.85\text{f}$ to $0.90\text{f}$). This causes droplet momentum to lag behind the local terrain gradients, shifting bank erosion downstream of bend apexes and forcing realistic downstream meander translation.
