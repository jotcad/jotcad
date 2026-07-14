# VFS Unit & Compliance Tests

This directory contains the unit tests for the VFS storage engines, resolver pipelines, and identity hashing.

## Test Suites

* `storage_compliance.test.js` — Verifies that all storage backends (`MemoryStorage`, `DiskStorage`, and browser `IndexedDBStorage`) behave identically regarding read, write, exists, metadata updates, and error handling.
* `vfs.test.js` — Core functional tests verifying provider registrations, schema lookups, and direct memory writes.
* `vfs_links.test.js` — Verifies strict link resolution rules (e.g., that text-based `vfs:/` strings are NOT guessed/followed, but formal metadata-driven links are followed recursively).
* `vfs_resolver.test.js` — Verifies recursion limits, loop checks, and local-only fallback conditions during resolution.

## Browser Testing

* [web/](file:///home/brian/github/jotcad_ez/fs/test/web) — Serves as a staging playground for verifying the VFS and `IndexedDBStorage` inside a browser environment (Service Workers and ESM modules).

## Execution

To execute the file system unit tests, run:
```bash
npm run test
```
The test runner executes these sequentially to prevent race conditions and port overlaps.
