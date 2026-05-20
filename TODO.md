# Project TODO List

## Compiler & Language
- [ ] **Fix `JotNumbersConsumer` Plural Consumption**: Update the compiler to allow operators expecting `jot:numbers` to consume Selectors that return plural number types (e.g., generator calls like `range`). Currently, the consumer only accepts raw numbers or literal arrays.
- [ ] **Fix `JotStringsConsumer` Plural Consumption**: Apply similar logic to strings for consistency across all plural types.

## Geometry Kernels
- [ ] **Standardize Plural Producers**: Audit C++ operators to ensure they correctly declare `jot:numbers` or `jot:strings` in their output schema if they return sequences.
