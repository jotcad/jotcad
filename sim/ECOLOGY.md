# Forest Ecology & Pedogenesis (Soil Construction)

This document details the vegetation models (grass vs. trees), seed dispersal, light competition, carbon cycles, and organic soil accumulation processes in JotCAD.

---

## 1. Plant Functional Types (PFTs)

To simulate succession, we model two competing vegetation types: Grass density $\rho_g \in [0, 1]$ and Tree density $\rho_t \in [0, 1]$.

| Parameter | Grass ($\rho_g$) | Trees ($\rho_t$) |
| :--- | :--- | :--- |
| **Growth Rate** | Fast (days to weeks) | Slow (years to decades) |
| **Soil Depth Requirement** | Shallow ($\ge 5\text{ cm}$) | Deep ($\ge 50\text{ cm}$) |
| **Root Depth** | Surface fibrous mat | Deep taproot anchors |
| **Light Competition** | Shade-tolerant / Understory | Overstory Canopy |

---

## 2. Growth & Environmental Suitability

Density changes for PFT $x \in \{g, t\}$ are modeled as:
$$\frac{\partial \rho_x}{\partial t} = G_x \cdot \Psi_{\text{moisture}, x} \cdot \Psi_{\text{soil}, x} \cdot \Psi_{\text{sun}, x} \cdot (1 - \rho_x) - M_x \cdot \rho_x$$
Where $G_x$ is growth rate, $M_x$ is decay rate, and the suitability coefficients are:

1. **Soil Suitability ($\Psi_{\text{soil}}$)**:
   - Grass: $\Psi_{\text{soil}, g} = 1 - e^{-h_{\text{soil}} / 0.05}$
   - Trees: $\Psi_{\text{soil}, t} = 1 - e^{-h_{\text{soil}} / 0.50}$ (restricts trees on bare bedrock).
2. **Moisture Suitability ($\Psi_{\text{moisture}}$)**:
   $$\Psi_{\text{moisture}, x} = \exp\left(-\frac{(M - M_{\text{opt}, x})^2}{2\sigma_x^2}\right)$$
   Where $M$ is soil moisture (sum of infiltration and exfiltration).
3. **Light Competition (Trees vs. Grass)**:
   Trees grow taller. They shade out grass beneath their canopy:
   $$\Psi_{\text{sun}, g} = \Psi_{\text{sun, surface}} \cdot (1 - \rho_t)$$
   $$\Psi_{\text{sun}, t} = \Psi_{\text{sun, surface}}$$

---

## 3. Carbon Sequestration & Humification

Vegetation traps carbon dioxide ($CO_2$) to build the soil organic layer, creating fertile soil from weathered rock.

### A. Net Primary Productivity (NPP)
Carbon capture rate (NPP) is proportional to active growth:
$$\text{NPP}_x = G_{\text{carbon}} \cdot \rho_x \cdot \Psi_{\text{moisture}, x} \cdot \Psi_{\text{sun}, x} \cdot CO_2(t)$$
Where $CO_2(t)$ is the atmospheric carbon level.

### B. Organic Soil Accumulation
Dead biomass (litterfall $L$) accumulates and decomposes into stable humus (organic soil layer $h_{\text{organic}}$):
$$L = M_g \rho_g + M_t \rho_t$$
$$\frac{\partial h_{\text{organic}}}{\partial t} = \eta_{\text{hum}} \cdot L - K_{\text{decomp}} \cdot h_{\text{organic}}$$
* $\eta_{\text{hum}}$: Fraction of litter converted to humus (humification factor).
* $K_{\text{decomp}}$: Decomposer respiration rate (carbon returned to the atmosphere as $CO_2$).

Total soil thickness is the sum of mineral and organic layers:
$$h_{\text{soil}} = h_{\text{mineral}} + h_{\text{organic}}$$

---

## 4. Ecology-Hydrology Feedbacks

* **Surface Shielding**: Grass reduces surface erodibility exponentially:
  $$K_e(\rho_g, \rho_t) = K_{e,\text{base}} \cdot e^{-(\lambda_g \rho_g + \lambda_t \rho_t)} \quad (\text{where } \lambda_g \gg \lambda_t)$$
* **Slope Anchoring**: Tree taproots increase soil cohesion:
  $$c'(\rho_g, \rho_t) = \chi_g \rho_g + \chi_t \rho_t \cdot \min(1, h_{\text{soil}}) \quad (\text{where } \chi_t \gg \chi_g)$$
* **Infiltration**: Root channels increase soil porosity, increasing infiltration capacity $f$:
  $$f(\rho_g, \rho_t) = f_{\text{base}} \cdot (1 + \eta_g \rho_g + \eta_t \rho_t)$$
