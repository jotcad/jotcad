# JotCAD Integration Tests

This directory contains end-to-end integration and system tests for the JotCAD geometry processing pipeline, testing VFS networking, compiler evaluation, step import/export, STL export, and rendering.

## Directory Structure

*   [actual/](file:///home/brian/github/jotcad/integration/actual): Output directory containing generated test artifacts like PNG snapshots and exported files.
*   [puppeteer/](file:///home/brian/github/jotcad/integration/puppeteer): Browser-based automated UI testing using Puppeteer.

## Key Test Files

*   [step_import.test.js](file:///home/brian/github/jotcad/integration/step_import.test.js): Verifies STEP file parsing, triangulation, and export protocol flow using a mock STEP fallback.
*   [step_real_import.test.js](file:///home/brian/github/jotcad/integration/step_real_import.test.js): Imports a real-world STEP file, processes it through OpenCascade, and generates a rendered PNG snapshot.
*   [prompt_staircase.test.js](file:///home/brian/github/jotcad/integration/prompt_staircase.test.js): Verifies VFS schema-to-LLM prompt translation and executes a compiled JOT staircase script, rendering a PNG snapshot.
