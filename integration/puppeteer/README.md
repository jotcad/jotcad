# JotCAD Puppeteer Integration Tests

This directory contains browser-based end-to-end integration tests using Puppeteer. These tests automate browser sessions to interact with JotCAD's rendering interfaces, validating UI logic, canvas updates, and texture mapping.

## Key Tests

- **[texture.test.js](texture.test.js)**: Verifies the loading, binding, and rendering of textures on 3D geometry in the Canvas/WebGL context. Routes browser logs and page errors to standard stdout/stderr for detailed reporting.
