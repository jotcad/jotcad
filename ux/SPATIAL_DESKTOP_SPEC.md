# JotCAD Spatial Desktop Specification

This document defines the architecture for the JotCAD "Spatial Desktop" (Blackboard) UI, providing a consistent, cross-device experience that scales from mobile to 4K displays.

## 1. The Core Metaphor
The UI is divided into two distinct logical layers:

### 1.1 The Blackboard (Spatial Board Space)
- **Role**: An infinite, zoomable, and pannable canvas.
- **Responsibility**: Hosting **Desktop Icons** and **Folders**.
- **Interaction**:
    - **Single Click/Tap**: Launch App (Open Window).
    - **Long Press / Right Click**: Context Menu (Delete, Rename, Change Icon).
    - **Drag**: Pan the blackboard (when not dragging an icon).
    - **Pinch/Scroll**: Zoom in/out.

### 1.2 The App Layer (Fixed Screen Space)
- **Role**: A flat layer above the blackboard for focused work.
- **Responsibility**: Hosting **Windows**.
- **Responsive Maximization**:
    - On **Mobile**, windows auto-maximize to fill the viewport.
    - On **Desktop**, windows are draggable and overlapping.
    - **Zoom Independence**: Maximized windows are *not* affected by the blackboard zoom level, ensuring UI components (buttons, text) remain legible.

## 2. The `Worksheet` Abstraction
The `Worksheet` is the unified persistence and synchronization layer. It treats all workspace data as a single merging entity.

### 2.1 Storage Tiers
| Tier | Format | Sync Logic | Description |
| :--- | :--- | :--- | :--- |
| **Operators** | `.jot` (Text) | `diff3` | User source code and JOT scripts. |
| **Desktop** | `json` | `trimerge` | Icon positions, folder structure, and custom icons. |
| **Windows** | `json` | `trimerge` | Open apps, window positions, and z-index. |

### 2.2 Persistence Flow
1. **Mutation**: User moves an icon or edits code.
2. **Local Commit**: State is saved to `localStorage` immediately.
3. **Merge Queue**: Changes are batched and merged with `RemoteStorage` (Cloud) using the `Worksheet` logic.
4. **Broadcast**: Updated state is synced back to all connected devices.

## 3. Component Architecture

### 3.1 `WindowManager`
A high-level SolidJS component that manages the `openWindows` store.
- **Stacking**: Manages `z-index` based on "Focus" (last clicked).
- **Maximization**: Toggles a `fixed` CSS class that overrides local positions with viewport-filling dimensions.

### 3.2 `DesktopIcon`
A draggable component pinned to blackboard coordinates.
- **Selection**: Highlights on tap.
- **Activation**: Triggers `windowActions.open(type, id)`.

### 3.3 Folder System
- **Recursive Structure**: Icons can have `children` (other icons).
- **Activation**: Opens a window showing a grid of child icons.

## 4. Interaction Conventions (Android-style)
- **Primary Action (Single Tap)**: Always "Open" or "Select".
- **Secondary Action (Long Press)**: Shows a floating action bar or menu.
- **Pan Conflict**: Panning the background is only inhibited if a pointer is captured by a draggable component (Icon or Window Header).

## 5. State Schema Example

```json
{
  "desktop": {
    "icons": [
      { "id": "mesh_graph", "type": "app", "label": "Mesh Graph", "x": 100, "y": 100 },
      { "id": "my_folder", "type": "folder", "label": "Projects", "x": 200, "y": 100, "children": [...] }
    ]
  },
  "windows": [
    { 
      "id": "mesh_graph", 
      "type": "mesh_graph_app", 
      "pos": { "x": 50, "y": 50 }, 
      "size": { "w": 800, "h": 600 },
      "isMaximized": false,
      "zIndex": 10
    }
  ]
}
```
