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
* **Concept Borrowed**: Upstream flow curvature lag. In nature, meanders translate downstream because helical secondary flow takes some distance to grow and strike the bank.
* **Refinement**: While `meanderpy` uses a 1D centerline spline convolution model, we represent this physical lag in our 2D model by setting a high velocity `inertia` ($0.85\text{f}$ to $0.90\text{f}$). This causes droplet momentum to lag behind the local terrain gradients, shifting bank erosion downstream of bend apexes and forcing realistic downstream meander translation.

---

## 9. Hexagonal Grid Landscape Evolution

JotCAD also supports a data-driven **Hexagonal Grid Landscape Evolution** engine (implemented in C++ in the `flow/` directory) for generating large-scale procedural continent maps.

### 9.1 Hydrological Elements
*   **`HexPrecipitation`**: Spawns uniform annual rainfall based on the climate profile's `rain_rate` (e.g. $1.4\text{ m/yr}$ for highlands, $0.15\text{ m/yr}$ for desert).
*   **`HexEvaporation`**: Computes local cell evaporation using elevation-dependent temperature cooling:
    $$\text{temp} = \text{base\_temp} - \frac{H_{\text{soil}}}{\text{lapse\_rate\_divisor}}$$
    $$\text{PET} = K_{\text{evap}} \times \max(0.0, \text{temp})$$
    Actual evaporation reduces the surface water depth locally before routing.
*   **`HexSubsurface`**: Simulates unsaturated soil infiltration and lateral Darcy groundwater flow:
    *   **Threshold-Based Diffuse Recharge**: Rainfall only infiltrates down to the deep water table (`h_g`) if the local soil saturation ratio is $\ge 15\%$ (capillary storage limit). Below this threshold, rainwater remains entirely on the surface as runoff.
    *   **Focused Lake/River Recharge**: Any standing surface water greater than $25\text{ cm}$ (e.g. in lakes or major stream channels) bypasses the saturation limit and recharges the soil aquifer beneath it directly.
    *   **Darcy Flow**: Groundwater moves laterally down the hydraulic head gradient ($H_{\text{head}} = H_{\text{bedrock}} + h_{\text{groundwater}}$) according to the profile's lateral conductivity. If the water table rises above the surface, it exfiltrates as spring seepage.
*   **`HexRouting`**: Implements a **Two-Pass Component-Based Basin Water Balance** model to handle endorheic sinks and overflowing lakes cleanly on the hexagonal layout.
 
### 9.2 Two-Pass Basin Water Balance
Closed depressions (lake basins) are identified as labeled connected components of cells using Breadth-First Search (BFS) starting from the sinks up to the basin brims:
1.  **Pass 1 (Global Inflow Gathering)**: Routes water downhill to accumulate total inflows globally at each basin's lowest cell (outlet). This captures all runoff from surrounding hillsides.
2.  **Basin Mass Balance**: The total inflow volume $Q_{\text{in}}$ (in $\text{m}^3/\text{yr}$) reaching the basin is compared against the basin's total potential evaporation volume $Q_{\text{evap}}$ (the sum of cell PETs over the basin area).
    *   **Overflowing Basin ($Q_{\text{in}} \ge Q_{\text{evap}}$)**: The basin fills completely to its brim elevation. The surplus water volume ($Q_{\text{in}} - Q_{\text{evap}}$) is discharged from the spillway cell and routed downhill during Pass 2.
    *   **Non-Overflowing Basin ($Q_{\text{in}} < Q_{\text{evap}}$)**: The lake does not overflow. A single flat water level $H_{\text{water}}$ is calculated bottom-up by volume-filling such that:
        $$Q_{\text{in}} = \sum_{\text{flooded cells}} (\text{PET} \times \text{cell\_area})$$
        All cells within the basin have their downstream directions redirected towards the lake floor.
3.  **Pass 2 (Discharge Consolidation)**: Re-runs the flow routing using the corrected downstream directions and spillway discharges to calculate stable flow rates ($g.Q$) and surface water depths.
 
### 9.3 Visual Downhill Rendering Constraint
To prevent visual rendering anomalies where river channels appear to climb uphill over ridges (due to flat water elevations inside lakes), both the C++ pixel exporter (`hex_exporter.h`) and the HTML dashboard (`hex_visualizer.html`) enforce a visual downhill slope check:
*   A river segment between a cell and its downstream neighbor is **only drawn** if the neighbor's soil elevation is downhill or level:
    $$H_{\text{soil}}[\text{downstream}] \le H_{\text{soil}}[\text{current}] + 0.05\text{m}$$
*   This automatically hides the internal routing vectors inside lake bodies (since the path towards the outlet climbs the lake bed uphill to the spillway). Rivers stop cleanly at the lake shoreline, and outflow rivers start exactly at the spillway cell and flow downhill.
 
### 9.4 Climate-Specific Calibration
Climate parameters are defined dynamically per profile in `ClimateProfile` to govern long-term landscape evolution:
*   **Subtropical Highland**: 
    *   Rainfall: $1.4\text{ m/yr}$ | Base Temp: $25^\circ\text{C}$
    *   Evaporation Coefficient: $K_{\text{evap}} = 0.0024f$
    *   Subsurface: $15\%$ starting saturation, $25\%$ annual infiltration rate, $60\text{ m/yr}$ lateral conductivity.
    *   Result: Sustains lush vegetation ($57.6\%$) and active, overflowing river networks draining out to margins.
*   **Arid Desert**: 
    *   Rainfall: $0.15\text{ m/yr}$ | Base Temp: $35^\circ\text{C}$
    *   Evaporation Coefficient: $K_{\text{evap}} = 0.0060f$ (strong evaporative deficit of $0.21\text{ m/yr}$ in low valleys)
    *   Subsurface: $0\%$ starting saturation (depleted aquifer), $4\%$ annual infiltration rate, $250\text{ m/yr}$ lateral conductivity (coarse sand).
    *   Result: All river channels run dry ($0$ active river cells). Widespread diffuse recharge is blocked, restricting oases and lakes strictly to low depressions that pool focused runoff ($1.96\%$ total water coverage, representing highly realistic desert oasis ponds).

