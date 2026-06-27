# Geomorphology, Sediment Sorting & Glaciation

This document details the mechanical weathering (splintering), wind erosion (Aeolian), rock lithification, sediment sorting, and glacial processes modeled in JotCAD.

---

## 1. Weather Splintering (Physical Weathering)

Physical weathering splinters solid bedrock into loose, transportable mineral sediment ($h_{\text{mineral}}$) through frost wedging (freeze-thaw) and thermal expansion.

$$\text{Splintering Rate } (W_m) = K_{\text{splinter}} \cdot \Delta T_{\text{amp}} \cdot e^{-\alpha_{\text{buff}} \cdot h_{\text{soil}}}$$

* $K_{\text{splinter}}$: Rock vulnerability constant (granite is low, shale is high).
* $\Delta T_{\text{amp}}$: Temperature range (increased if cycles cross $0^\circ C$, driving ice expansion).
* $\alpha_{\text{buff}}$: Thermal buffering coefficient of soil thickness (bare rock splinters fastest).

---

## 2. Wind (Aeolian) Erosion

Wind transports fine dry particles (sand and silt), creating desert landscapes and dunes.

### A. Wind Shear & Transport Capacity
Wind transport capacity $Q_{\text{wind}}$ is proportional to the cube of friction velocity $u_*$ (Bagnold's formula) and is limited by soil moisture and vegetation:
$$Q_{\text{wind}} = K_{\text{wind}} \cdot (u_* - u_{*, t})^3 \cdot e^{-\kappa M_{\text{wet}}} \cdot e^{-\lambda_{\text{wind}}(\rho_g + \rho_t)}$$
* $u_{*, t}$: Threshold friction velocity to lift dry sand particles.
* $M_{\text{wet}}$: Soil moisture.
* $\rho_g, \rho_t$: Vegetation densities shielding the surface.

### B. Deposition (Dune Accumulation)
Wind deposits sand where velocity drops or in the wind-shadow of obstacles (hills):
$$\text{Deposition}_{\text{wind}} = Q_{\text{wind}} \cdot \max\left(0, 1 - \frac{u_*}{u_{*, t}}\right)$$

---

## 3. Sediment Sorting & Grain Sizes

Rivers and wind sort sediment by weight. We model four sediment grain size classes:
$$\text{Gravel } (d \ge 2\text{mm}) \quad \text{Sand } (2\text{mm} > d \ge 0.06\text{mm}) \quad \text{Silt } (0.06\text{mm} > d \ge 0.002\text{mm}) \quad \text{Clay } (d < 0.002\text{mm})$$

1. **Critical Shear Stress ($\tau_{c, x}$)**: The hydraulic force required to erode/lift a grain class $x$ is proportional to grain mass (Shields curve):
   $$\tau = \rho_{\text{water}} \cdot g \cdot d_{\text{water}} \cdot S$$
   * If water shear stress $\tau > \tau_{c, x}$, class $x$ is eroded from soil into the river's active sediment pool.
2. **Sorting Deposition**: When stream velocity slows down (entering lakes or flat valleys):
   - Gravel deposits first (forming gravel bars).
   - Sand and silt deposit in floodplains (creating fertile soils).
   - Clay travels farthest, depositing in calm basins/deltas.

---

## 4. Rock Formation (Lithification)

Over geological timescales, loose sediment buried at the bottom of basins compacts back into solid bedrock under weight:
$$P_{\text{lith}} = K_{\text{lith}} \cdot h_{\text{mineral}} \cdot (\rho_{\text{bulk}} \cdot g \cdot h_{\text{soil}})$$
Where loose mineral soil ($h_{\text{mineral}}$) is compressed and converted into bedrock ($H_{\text{bedrock}}$) at the soil boundary.

---

## 5. Glacial Erosion

In high-altitude cold zones, accumulated snow compresses into ice, flowing downhill and carving U-shaped valleys.

### A. Glacial Flow
Ice thickness $h_{\text{ice}}$ flows downhill under gravity. The velocity $v_{\text{ice}}$ is modeled using Glen's Flow Law:
$$v_{\text{ice}} = A \cdot \tau_{\text{ice}}^3 \cdot h_{\text{ice}}$$
Where $\tau_{\text{ice}} = \rho_{\text{ice}} \cdot g \cdot h_{\text{ice}} \cdot \sin(\theta)$ is the basal shear stress.

### B. Glacial Abrasion & Quarrying
Glaciers erode terrain by grinding rock (abrasion) and plucking blocks (quarrying):
$$\frac{\partial H_{\text{terrain}}}{\partial t} = -K_{\text{glacial}} \cdot v_{\text{ice}}^2$$
This high-energy erosion removes soil and Bedrock, carving wide **U-shaped valleys** with steep walls and flat bottoms.
