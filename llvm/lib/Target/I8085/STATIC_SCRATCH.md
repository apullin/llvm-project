# I8085 Static Scratch

`I8085StaticScratch` is an optional MachineFunction pass that redirects selected
8-bit and 16-bit spill slots from stack-relative accesses to private static RAM
symbols. This is intended for RAM-resident firmware or OS code where fixed
workspace bytes are acceptable and code size matters.

Enable it with:

```sh
-mllvm -i8085-static-scratch-bytes=N
```

The pass emits one private global per converted frame object. Those globals are
placed in `.bss.i8085.scratch`, a target-specific BSS subsection. Linker scripts
that already collect `.bss*` will normally place the scratch storage with other
zero-initialized RAM and keep it out of flat ROM/code images. A tighter linker
script can still place `.bss.i8085.scratch` explicitly.

## Safety model

Enabling this option promises exclusive access to each function's scratch while
its converted slots are live. In particular, an ISR or another task must not
enter that function in that interval. The compiler cannot prove asynchronous
exclusivity. Single-threaded foreground code with no interrupt re-entry satisfies
this execution-model requirement; `norecurse` alone does not.

Within that execution model, the default mode is synchronous-call-safe. It
converts only supported spill slots whose values are not live across a call,
so ordinary recursive calls cannot overwrite a live converted slot. There is no
default-on scratch conversion and no implicit interrupt-safety guarantee.

Slots that are live across a call can be converted only when the function has an
explicit nonreentrant contract:

```llvm
attributes #0 = { "i8085-nonreentrant" }
```

For experiments, the same contract can be supplied from the command line:

```sh
-mllvm -i8085-static-scratch-assume-nonreentrant-function=foo
-mllvm -i8085-static-scratch-assume-nonreentrant
```

This is stronger than LLVM's `norecurse` attribute. `norecurse` only says that a
function does not participate in recursion through calls visible to LLVM. It
does not prove that interrupts, callbacks, cooperative scheduling, or foreign
entry points cannot re-enter the function while its scratch bytes are live.

Known direct recursive SCCs remain blocked by default, even with the
nonreentrant command-line assumptions. The research escape hatch is:

```sh
-mllvm -i8085-static-scratch-unsafe-recursive
-mllvm -i8085-static-scratch-allow-known-recursive
```

Use those only when the whole program's execution model makes the scratch
storage exclusive for the lifetime of the converted activation.

## Debugging

To audit conversions:

```sh
-mllvm -i8085-static-scratch-dump
```

The dump reports the function, frame index, size, stack offset, spill/aliasing
flags, whether the slot is live across a call, and the emitted scratch symbol.
Use these narrowing options when reducing a failure:

```sh
-mllvm -i8085-static-scratch-only-function=foo
-mllvm -i8085-static-scratch-skip-function=foo
-mllvm -i8085-static-scratch-only-frame-index=0
```
