# Blackboard Navigation & Controls

This directory manages interaction mapping and camera controls for the infinite 2D spatial workspace (Blackboard).

## Core Responsibility
Providing zoom and pan controls for desktop, trackpad, and touchscreen gestures over the blackboard canvas while isolating other interactive viewport containers.

## Files
- [BlackboardControls.js](./BlackboardControls.js): Handles gesture listening, delta mapping (wheel, trackpad pinch, multi-touch pinch), and boundary isolation for window components.
