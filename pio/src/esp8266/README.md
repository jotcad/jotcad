# ESP8266 VFS Client Library

This directory contains the PlatformIO C++ Virtual File System (VFS) client library customized specifically for the **ESP8266** platform (e.g., WeMos D1, NodeMCU).

## Responsibility

Because the ESP8266 is a single-core microcontroller running without FreeRTOS by default, this library implements a strictly **cooperative, asynchronous, non-blocking** VFS implementation. It avoids any multi-threaded primitives (using a lightweight dummy `Mutex` instead of `std::recursive_mutex`) and leverages the native ESP8266 hardware-accelerated `<Hash.h>` library for stable content hashing (SHA-256).

## Files Summary

- [vfs.h](file:///home/brian/github/jotcad/pio/src/esp8266/vfs.h): Defines the cooperative `VFS` class, the `Peer` interface, and the custom dummy `Mutex` structure.
- [vfs.cpp](file:///home/brian/github/jotcad/pio/src/esp8266/vfs.cpp): Implements the non-blocking polling and network event loop (`tick()`).
- [vfs_types.h](file:///home/brian/github/jotcad/pio/src/esp8266/vfs_types.h): Declares common structural types (`CID`, `Selector`), serialization/deserialization utilities for JCB, and hashing signatures.
- [vfs_types.cpp](file:///home/brian/github/jotcad/pio/src/esp8266/vfs_types.cpp): Implements JotCAD Canonical Binary (JCB) formatting, base64 encoding/decoding, and ESP8266-specific SHA-256 hashing.
