# ESP32 VFS Client Library

This directory contains the PlatformIO C++ Virtual File System (VFS) client library customized specifically for the **ESP32** platform.

## Responsibility

The ESP32 platform is a dual-core microcontroller that supports robust multi-threading via FreeRTOS. This library implements a standard `VFS` client utilizing standard C++ synchronization primitives (`std::recursive_mutex`) and cryptographic libraries (`mbedtls` for SHA-256 hash generation).

## Files Summary

- [vfs.h](file:///home/brian/github/jotcad/pio/src/esp32/vfs.h): Defines the `VFS` class, the `Peer` interface, and necessary network abstractions.
- [vfs.cpp](file:///home/brian/github/jotcad/pio/src/esp32/vfs.cpp): Implements network event processing, non-blocking polling, and operations dispatching.
- [vfs_types.h](file:///home/brian/github/jotcad/pio/src/esp32/vfs_types.h): Declares structural types (`CID`, `Selector`), serialization/deserialization utilities for JCB, and hashing signatures.
- [vfs_types.cpp](file:///home/brian/github/jotcad/pio/src/esp32/vfs_types.cpp): Implements JCB binary encoding, base64 operations, and standard `mbedtls` SHA-256 hashing.
