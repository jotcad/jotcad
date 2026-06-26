# Desktop Environment Components

This directory contains components that constitute the JotCAD Spatial Desktop, coordinating windows, desktop icons, and custom status widgets.

## Key Components

-   `IconShell.jsx`: Base drag-and-drop glassmorphism container for all desktop items.
-   `CounterWidget.jsx`: Ephemeral status widget showing real-time ticks from the mesh-connected ESP32 sensor.
-   `RgbLedWidget.jsx`: Draggable status and glow widget representing the RGB LED control node.
-   `RgbLedWindow.jsx`: Interactive window containing RGB LED control sliders and quick color presets.
-   `DesktopIcon.jsx`: Standard clickable, draggable shortcut to apps or folder windows.
-   `Window.jsx`: Complex drag-and-drop window container supporting maximized portal view, title-bar actions, and 44x44px mobile touch targets.
-   `WindowManager.jsx`: Controls window layer ordering and rendering.
-   `FolderWindow.jsx`: Standard folder browser view inside a window.
