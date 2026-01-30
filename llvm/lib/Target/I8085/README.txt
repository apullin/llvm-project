//===-- README.txt - Notes for I8085 codegen ------------------------------===//

This target is focused on freestanding embedded use.

Debug / Unwind policy (current):
- DWARF CFI is emitted to describe CFA changes (stack pointer based).
- Callee-saved registers are currently empty, so CFI is limited to CFA.
- Exception handling is not supported (no personality/LSDA/runtime).
  Use -fno-exceptions/-fno-unwind-tables for now.

Minimal debug-line sanity is covered by CodeGen tests that check emitted
.file/.loc directives and .debug_line section presence.

//===---------------------------------------------------------------------===//

Open items and improvement ideas live in docs/STATUS.md.
