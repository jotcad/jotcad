# Climatology, Microclimate & Environmental Disturbances

This document details the climatological models (temperature lapse rates, solar insolation, aspect influence) and landscape disturbances (wildfires) modeled in JotCAD.

---

## 1. Temperature & Elevation (Lapse Rate)

Air temperature drops with elevation. This creates environmental zones that govern vegetation growth limits (tree lines) and snow accumulation.

### A. The Lapse Rate Equation
Local temperature $T(x, y)$ is calculated from sea-level temperature $T_{\text{sea\_level}}$ and local elevation $H_{\text{terrain}}(x, y)$:
$$T(x,y) = T_{\text{sea\_level}} - \Gamma \cdot H_{\text{terrain}}(x, y)$$
* $\Gamma$: Environmental lapse rate (approx. $0.0065^\circ\text{C / m}$ or $6.5^\circ\text{C}$ per $\text{km}$).

### B. Ecological Boundaries
- **Tree Line**: Trees cannot grow if summer temperature $T_{\text{summer}} < 10^\circ\text{C}$. This enforces:
  $$\Psi_{\text{soil}, t} = 0 \quad (\text{if } H_{\text{terrain}} > H_{\text{treeline}})$$
- **Snow Line**: Regions where winter temperature remains below $0^\circ\text{C}$ accumulate snow, feeding glaciers.

---

## 2. Solar Insolation & Slope Aspect

Slope aspect (the compass direction a slope faces) and slope angle control local solar radiation, driving evaporation and vegetation species separation.

### A. Solar Radiation Vector
Let $\vec{L}$ be the unit vector pointing toward the sun, and $\vec{n}$ be the local surface normal of the terrain:
$$\text{Insolation } (I) = I_0 \cdot \max\left(0, \vec{n} \cdot \vec{L}\right)$$

### B. Aspect Index (North vs. South Slopes)
In the Northern Hemisphere, south-facing slopes receive the most direct sun, while north-facing slopes remain shaded:
$$\text{Aspect Index } (\mathcal{A}) = \cos\left(\text{Aspect Angle} - 180^\circ\right)$$
* $\mathcal{A} \approx 1.0$: South-facing (hot, dry, favors grass).
* $\mathcal{A} \approx -1.0$: North-facing (cool, moist, favors thick forests).

This shifts the moisture and light suitability curves for vegetation:
$$M_{\text{opt, trees}}(\text{north-facing}) < M_{\text{opt, trees}}(\text{south-facing})$$

---

## 3. Wildfire Dynamics

Fires clear biomass, release stored carbon back to the atmosphere, and leave the terrain vulnerable to extreme erosion events.

### A. Ignition Probability ($P_{\text{ignite}}$)
A cell $(i, j)$ can ignite based on temperature $T$, soil dryness, and fuel (vegetation density):
$$P_{\text{ignite}} = K_{\text{fire}} \cdot T \cdot (1 - M_{\text{wet}}) \cdot (\rho_g + \rho_t)$$

### B. Fire Propagation
Fire spreads to neighboring cells based on wind direction vector $\vec{v}_{\text{wind}}$ and terrain slope (fire spreads faster uphill):
$$V_{\text{spread}} = V_0 \cdot \left(1 + \vec{v}_{\text{wind}} \cdot \vec{d}_{\text{neighbor}}\right) \cdot \max\left(0.5, \frac{\Delta H}{L}\right)$$
* If a cell burns, its vegetation density resets: $\rho_g = 0, \rho_t = 0$.
* Soil organic matter $h_{\text{organic}}$ is reduced by burning.
* The carbon is returned to the atmospheric $CO_2$ pool.

### C. Post-Fire Erosion Spike
Since vegetation is destroyed, soil cohesion $c'$ drops and erodibility $K_e$ spikes to maximum. Subsequent rain causes massive soil stripping and mudslides (pore-pressure failures).
