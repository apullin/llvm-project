# i8085 Smoke Tests

Small end-to-end programs for the i8085 toolchain. These are intended for local
validation of the clang → ld.lld → ELF flow and for quick simulator checks.

## Layout

- `run-smoke.sh`: builds and (optionally) runs all programs
- `smoke.c`: minimal memory-write loop
- `init.c`: .data copy + .bss zeroing sanity
- `recursion.c`: recursive calls + stack frames
- `heap.c`: simple bump allocator usage
- `irq.c` / `irq.S`: interrupt injection test (RST 5.5)

## Requirements

This script expects the top-level layout used by the i8085 worktree:

```
<repo-root>/llvm-project
<repo-root>/sysroot
<repo-root>/i8085-trace
```

If your layout differs, set env vars for `SYSROOT` and `TRACE`.

## Usage

```
./run-smoke.sh
```

Each program writes a small result to RAM at `0x0100`. The simulator dumps that
memory range after execution.

## Environment overrides

- `CLANG`: path to clang (default: `llvm-project/build-clang-8085/bin/clang`)
- `SYSROOT`: sysroot root (default: `../sysroot`)
- `LINKER`: linker script (default: `SYSROOT/ldscripts/i8085-32kram-32krom.ld`)
- `OBJCOPY`: llvm-objcopy path
- `TRACE`: i8085-trace binary
- `OUTDIR`: build output directory
