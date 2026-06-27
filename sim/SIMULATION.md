# Landscape Hydrology & Erosion Simulation

This document outlines the mathematical models, algorithmic execution loops, and coupling dynamics for simulating rivers, soil, and forest formation inside the JotCAD simulation framework.

---

## 1. Surface Hydrology & River Incision

To simulate the formation of rivers on a heightmap $H_{\text{terrain}}(x, y)$, we model how surface water accumulates and erodes the bedrock and soil.

### A. Flow Routing (D8 Algorithm)
Water flows downhill in the direction of the steepest gradient. In a grid-based system:
1. For each cell $(i, j)$, calculate the slope to each of its 8 neighbors:
   $$S_d = \frac{H_{\text{terrain}}(i, j) - H_{\text{terrain}}(\text{neighbor}_d)}{L_d}$$
   Where $L_d = 1$ for orthogonal neighbors and $L_d = \sqrt{2}$ for diagonal neighbors.
2. Select the neighbor $d$ with the largest positive $S_d$.
3. Distribute the cell's water volume to that neighbor.
4. **Flow Accumulation ($A$)**: By recursively routing water, calculate the drainage area (total upstream cells flowing through each cell). Cells where $A > A_{\text{threshold}}$ are identified as active stream channels.

### B. River Incision (Stream Power Law)
The rate at which the riverbed is carved by running water is governed by the Stream Power Law:
$$\frac{\partial H_{\text{terrain}}}{\partial t} = -K_e \cdot A^m \cdot S^n$$
* $K_e$: Erodibility coefficient (scaled by soil/bedrock resistance and root anchoring).
* $A$: Flow accumulation (proxy for water discharge).
* $S$: Local slope gradient.
* $m, n$: Empirical exponents (typically $m \approx 0.5$, $n \approx 1.0$).

---

## 2. Subsurface Hydrology & Groundwater Table

To model water storage, springs, and saturated soil, we simulate the groundwater aquifer.

### A. Infiltration
Rainfall $R$ is split into surface runoff $R_{\text{surface}}$ and infiltration $R_{\text{infiltrate}}$ based on Horton's infiltration capacity $f$:
$$f = f_c + (f_0 - f_c)e^{-k_f t}$$
$$R_{\text{infiltrate}} = \min(R, f)$$
$$R_{\text{surface}} = R - R_{\text{infiltrate}}$$

### B. Groundwater Flow (Darcy's Law)
Let $H_{\text{gw}}(x, y)$ be the height of the groundwater table. The hydraulic head $H$ is the total height:
$$H_{\text{head}} = H_{\text{bedrock}} + h_{\text{gw}}$$
The specific discharge vector $\vec{q}$ (subsurface flow) is given by Darcy's law:
$$\vec{q} = -K_{\text{sat}} \cdot \nabla H_{\text{head}}$$
Where $K_{\text{sat}}$ is the saturated hydraulic conductivity of the soil.

### C. Exfiltration (Springs & Baseflow)
If groundwater routing causes $H_{\text{head}} > H_{\text{terrain}}$ in any cell $(i, j)$:
1. Water seeps out (exfiltrates) to the surface:
   $$W_{\text{seep}} = H_{\text{head}} - H_{\text{terrain}}$$
2. The water table is capped to the surface:
   $$H_{\text{head}} = H_{\text{terrain}}$$
3. The seepage volume $W_{\text{seep}}$ is added to the surface water routing pool, creating natural springs and sustaining baseflow in rivers.

---

## 3. Bank Stability & Pore Pressure Collapse

Unconsolidated soil on riverbanks collapses under gravity, especially when saturated.

### A. Pore Water Pressure
The shear strength of soil $\tau_f$ decreases as water fills the pore spaces, creating buoyancy that pushes grains apart (Mohr-Coulomb failure criterion):
$$\tau_f = c' + (\sigma - u) \tan \phi'$$
* $c'$: Soil cohesion (increased by vegetation root density $\rho_v$).
* $\sigma$: Normal stress (soil weight).
* $u$: **Pore water pressure** (proportional to groundwater height $h_{\text{gw}}$).
* $\phi'$: Friction angle.

### B. Saturated Talus & Slumping Operator
On a heightmap, this is simulated using a wetness-dependent angle of repose:
1. Determine local saturation $S_{\text{sat}} = \frac{h_{\text{gw}}}{h_{\text{soil}}}$.
2. Calculate the dynamic maximum stable slope angle $\theta_{\text{max}}$:
   $$\theta_{\text{max}} = \theta_{\text{dry}} \cdot (1 - \gamma S_{\text{sat}}) + \lambda \rho_v$$
   Where $\gamma$ is the pore-pressure instability coefficient, and $\lambda$ represents root reinforcement.
3. **Slumping Step**: If the slope between adjacent cells $(i,j)$ and neighbor exceeds $\theta_{\text{max}}$:
   - Displace soil from the higher cell to the lower cell.
   - If the lower cell contains a river channel, the slumped soil is injected directly into the stream as *suspended sediment*, which the river transports downstream.

---

## 4. Coupled Iterative Simulation Pipeline

Each simulation frame executes the following sequence:

```
┌────────────────────────────────────────┐
│ 1. Weathering                          │  Convert Bedrock to Soil:
│    P = P0 * exp(-α * h_soil)           │  h_soil += P; H_bedrock -= P
└───────────────────┬────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────┐
│ 2. Infiltration & Runoff               │  Apply Horton's equation:
│    R_infiltrate = min(Rain, f)         │  Add infiltration to Groundwater
└───────────────────┬────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────┐
│ 3. Groundwater Routing                 │  Solve Darcy's Law:
│    q = -K_sat * ∇H_head                │  Move groundwater via virtual pipes
└───────────────────┬────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────┐
│ 4. Exfiltration (Springs)              │  If H_head > H_terrain:
│    W_seep = H_head - H_terrain         │  Convert excess to Surface Water
└───────────────────┬────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────┐
│ 5. Surface Water & Erosion             │  D8/D-Infinity flow routing.
│    dH_terrain/dt = -Ke * A^m * S^n     │  River incises bedrock/soil layers
└───────────────────┬────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────┐
│ 6. Wetness-Dependent Slumping          │  If slope > θ_max(S_sat, ρ_v):
│    Move soil to river channel          │  Inject slumped soil as sediment
└────────────────────────────────────────┘
```
