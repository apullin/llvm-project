# I8085 Linker Support Plan

This document outlines a minimal, staged plan to enable end-to-end linking for
I8085 ELF objects produced by LLVM/clang.

## Current State (Already in Tree)
- ELF machine ID: EM_I8085 in `llvm/include/llvm/BinaryFormat/ELF.h`.
- i8085 e_flags and relocation enums present.
- ELF object writer emits R_I8085_16 relocations.

## Goal
Enable a basic `clang -target i8085-unknown-elf` link producing an ELF image
with .text/.data/.bss and a simple entry symbol, using an LLVM-based linker.

## Option A: Add LLD ELF backend for I8085 (Preferred)
This keeps the toolchain self-contained and testable.

### Phase 1: Minimal LLD bring-up
- Add EM_I8085 handling in lld/ELF (arch enums, target selection).
- Implement relocation handling for the existing set (start with R_I8085_16).
- Define basic endianness/word size and default flags.
- Add a minimal linker script or built-in layout with .text/.data/.bss.
- Add a smoke test that links two objects and checks section map and symbol.

### Phase 2: Relocations and fixes
- Add any missing relocation types needed by codegen/MC (hi8/lo8 if used).
- Implement relaxation or disable it explicitly for now.
- Add tests for relocations in lld/ELF test suite.

### Phase 3: Usability
- Wire clang driver to prefer `ld.lld` for i8085 if available.
- Provide a default linker script in the resource dir or toolchain data.
- Add end-to-end clang->obj->link test (expect ELF header + entry symbol).

## Option B: External linker (temporary)
- Identify an existing i8085-compatible linker (if any).
- Add a clang driver hook to invoke it with a script.
- Use this only as a temporary bridge if LLD is delayed.

## Open Questions
- Define a default memory map (ROM/RAM split) for the linker script.
- Decide on startup/CRT objects and default entry symbol.
- Decide how to model `__stack`/`__heap` symbols for bare metal.

## Proposed Next Step
- Create a minimal LLD target stub for EM_I8085 and a single relocation
  (R_I8085_16), then add a small lld test that links two i8085 objects.
