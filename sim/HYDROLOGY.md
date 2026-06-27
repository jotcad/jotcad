# Landscape Hydrology & Channel Incision

This document details the hydrology models (surface water, groundwater, springs, and bank stability) used to simulate river networks and hillslope processes in JotCAD.

---

## 1. Surface Hydrology

Surface water flow determines channel development and valley incision.

### A. Flow Routing (D8 Algorithm)
Water moves downhill across the terrain heightmap $H_{\text{terrain}}(x, y)$ toward the lowest neighboring cell.
1. For each cell $(i, j)$, calculate the gradient to its 8 neighbors:
   $$S_d = \frac{H_{\text{terrain}}(i, j) - H_{\text{terrain}}(\text{neighbor}_d)}{L_d}$$
   Where $L_d = 1$ for orthogonal neighbors and $L_d = \sqrt{2}$ for diagonal neighbors.
2. The cell drains into the neighbor with the maximum positive slope $S_d$.
3. **Flow Accumulation ($A$)**: By traversing drainage directions from high to low elevations, calculate the total upslope drainage area for each cell. Active stream channels occur where $A > A_{\text{threshold}}$.

### B. Channel Incision (Stream Power Law)
Running water carves channels into the bedrock and soil layers. The rate of incision is modeled as:
$$\frac{\partial H_{\text{terrain}}}{\partial t} = -K_e \cdot A^m \cdot S^n$$
* $K_e$: Erodibility coefficient (mitigated by soil strength and root density).
* $A$: Flow accumulation (proxy for discharge volume).
* $S$: Local slope gradient.
* $m, n$: Empirical exponents (typically $m \approx 0.5$, $n \approx 1.0$).

---

## 2. Subsurface Hydrology & Groundwater

Groundwater governs soil moisture, spring discharge, and slope stabilization.

### A. Infiltration
Rainfall $R$ is split into surface runoff $R_{\text{surface}}$ and soil infiltration $R_{\text{infiltrate}}$ using Horton's equation:
$$f = f_c + (f_0 - f_c)e^{-k_f t}$$
$$R_{\text{infiltrate}} = \min(R, f)$$
$$R_{\text{surface}} = R - R_{\text{infiltrate}}$$
* $f_0, f_c$: Initial and saturated infiltration capacities (both increased by vegetation cover).

### B. Groundwater Aquifer Flow (Darcy's Law)
The groundwater table elevation is represented as $H_{\text{gw}}(x, y)$. The total hydraulic head is:
$$H_{\text{head}} = H_{\text{bedrock}} + h_{\text{gw}}$$
The specific groundwater flow flux vector $\vec{q}$ is:
$$\vec{q} = -K_{\text{sat}} \cdot \nabla H_{\text{head}}$$
Where $K_{\text{sat}}$ is the saturated hydraulic conductivity of the soil.

### C. Exfiltration (Springs & Baseflow)
When the hydraulic head exceeds the ground surface ($H_{\text{head}} > H_{\text{terrain}}$):
1. Groundwater discharges onto the surface:
   $$W_{\text{seep}} = H_{\text{head}} - H_{\text{terrain}}$$
2. The water table is clamped to the surface:
   $$H_{\text{head}} = H_{\text{terrain}}$$
3. The exfiltrated seepage $W_{\text{seep}}$ is routed into the surface water pool, sustaining streams during dry periods.

---

## 3. Bank Stability & Pore Pressure

Saturated soil has reduced friction, causing riverbanks and hillsides to slump.

### A. Pore Water Pressure
Soil shear strength $\tau_f$ is governed by the Mohr-Coulomb equation:
$$\tau_f = c' + (\sigma - u) \tan \phi'$$
* $c'$: Soil cohesion (reinforced by roots).
* $\sigma$: Normal stress (soil weight).
* $u$: **Pore water pressure** (buoyancy force, proportional to soil saturation $S_{\text{sat}}$).
* $\phi'$: Internal friction angle.

### B. Slumping Operator
On a heightmap, slopes exceeding the moisture-dependent angle of repose collapse:
1. Compute the dynamic maximum stable slope $\theta_{\text{max}}$:
   $$\theta_{\text{max}} = \theta_{\text{dry}} \cdot (1 - \gamma S_{\text{sat}}) + c'(\rho_v)$$
   Where $S_{\text{sat}} = h_{\text{gw}} / h_{\text{soil}}$, $\gamma$ is the pore-pressure instability coefficient, and $c'(\rho_v)$ is root cohesion.
2. **Talus Redistribution**: If the slope to a neighbor exceeds $\theta_{\text{max}}$, move excess soil downhill.
3. If soil falls into a stream channel, it is added to the channel's suspended sediment load.
