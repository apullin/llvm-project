# RUN: python3 %s %llvm_mc

import re
import subprocess
import sys

if len(sys.argv) < 2:
    print("usage: opcode-roundtrip.py <llvm-mc>")
    sys.exit(1)

LLVM_MC = sys.argv[1]

THREE_BYTE = {
    0x01, 0x11, 0x21, 0x31,  # LXI
    0x22, 0x2A, 0x32, 0x3A,  # SHLD/LHLD/STA/LDA
    0xC3, 0xC2, 0xCA, 0xD2, 0xDA, 0xE2, 0xEA, 0xF2, 0xFA,  # JMP/Jcc
    0xCD, 0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC, 0xF4, 0xFC,  # CALL/Ccc
}

TWO_BYTE = {
    0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE,  # ALU imm8
    0xDB, 0xD3,  # IN/OUT
}

EXPECTED_INVALID = {
    0x08, 0x10, 0x18, 0x28, 0x38,  # 8080 unused opcodes
    0xCB, 0xD9, 0xDD, 0xED, 0xFD,  # 8080/Z80 unused opcodes
}

ENC_RE = re.compile(r"encoding: \[(.*?)\]")


def instr_size(opc: int) -> int:
    if opc in THREE_BYTE:
        return 3
    if (opc & 0xC7) == 0x06:
        return 2  # MVI r, imm8
    if opc in TWO_BYTE:
        return 2
    return 1


def run_mc(args, input_text: str):
    p = subprocess.run(
        [LLVM_MC] + args,
        input=input_text.encode(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    return p.stdout.decode(), p.stderr.decode()


def disassemble_one(opc: int):
    size = instr_size(opc)
    bytes_list = [opc] + [0x00] * (size - 1)
    data = " ".join(f"0x{b:02x}" for b in bytes_list) + "\n"
    out, err = run_mc(["-triple=i8085", "-disassemble"], data)

    if "invalid instruction encoding" in err:
        return None, bytes_list, err

    # Pick the last non-empty, non-directive line.
    instr_line = None
    for line in out.splitlines():
        s = line.strip()
        if not s or s.startswith("."):
            continue
        instr_line = s
    return instr_line, bytes_list, err


def assemble_and_get_bytes(asm_line: str):
    out, err = run_mc(["-triple=i8085", "-show-encoding"], asm_line + "\n")
    if err:
        raise RuntimeError(f"assembler error for '{asm_line}': {err.strip()}")
    for line in out.splitlines():
        m = ENC_RE.search(line)
        if not m:
            continue
        bytes_txt = m.group(1)
        parts = [p.strip() for p in bytes_txt.split(",") if p.strip()]
        return [int(p, 16) for p in parts]
    raise RuntimeError(f"no encoding found for '{asm_line}'")


invalid = set()
for opc in range(0x100):
    asm_line, expected_bytes, err = disassemble_one(opc)
    if asm_line is None:
        invalid.add(opc)
        continue

    actual_bytes = assemble_and_get_bytes(asm_line)
    if actual_bytes != expected_bytes:
        raise SystemExit(
            f"round-trip mismatch for 0x{opc:02x}: '{asm_line}' -> {actual_bytes},"
            f" expected {expected_bytes}"
        )

if invalid != EXPECTED_INVALID:
    missing = EXPECTED_INVALID - invalid
    extra = invalid - EXPECTED_INVALID
    msg = []
    if missing:
        msg.append("missing invalid opcodes: " + ", ".join(f"0x{x:02x}" for x in sorted(missing)))
    if extra:
        msg.append("unexpected invalid opcodes: " + ", ".join(f"0x{x:02x}" for x in sorted(extra)))
    raise SystemExit("; ".join(msg))
