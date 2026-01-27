# I8085 Backend Status

This file tracks feature completeness for the LLVM I8085 backend and related
clang/runtime support. It is intended as a living checklist.

## Codegen
- [x] i8/i16 arithmetic: add/sub/and/or/xor, compares, branches
- [x] i8/i16 extend/trunc (zext/sext/trunc)
- [x] stack argument lowering (SP-relative stores)
- [x] large frame offsets (no 6-bit offset restriction)
- [x] i32 lowering coverage beyond basic add/sub (audit all i32 pseudos)
- [x] i64/u64 lowering via libcalls (mul/div/rem/shift)
- [ ] varargs ABI validation with real C frontend
- [ ] tail calls
- [ ] inline asm constraints and register allocation stress tests

## MC / Asm / Disasm
- [x] opcode roundtrip and MC coverage for core instruction groups
- [x] RST encoding validation
- [x] ELF relocation emission for absolute 16-bit fixups
- [ ] symbol modifiers / target exprs (hi8/lo8/pm) wiring for i8085
- [x] assembler diagnostics coverage for illegal operands/ranges

## ABI / Runtime
- [x] libcall names set for mul/div/rem (signed + unsigned)
- [x] minimal compiler-rt builtins for mul/div/rem (signed + unsigned)
- [ ] calling convention doc + validation tests for aggregates/struct returns
- [x] startup/CRT objects (data copy + bss + stack)
- [x] default linker script

## Clang Driver
- [x] TargetInfo and minimal toolchain stub
- [x] sysroot / include path conventions (resource-dir sysroot)
- [x] driver tests for `-target i8085-unknown-elf`

## Tooling / Tests
- [x] CodeGen regression tests for call args and large stack
- [x] MC relocation tests
- [x] end-to-end clang -> asm -> object smoke test
- [x] end-to-end link smoke test (lld)

## Known Gaps / Risks
- Frame/stack ABI is minimal; verify with simulator once available.
- 32-bit lowering uses pseudo-registers with zero-page memory; needs audit.
- No floating point support.
- libc/libgcc in sysroot are stubs; real runtime still needed.
