# RUN: llvm-mc -triple=i8085-unknown-elf -mattr=+undoc -show-encoding < %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=i8085-unknown-elf -mattr=+undoc -filetype=obj < %s | \
# RUN:   llvm-objdump -d --mattr=+undoc - | FileCheck %s --check-prefix=DISASM
# RUN: not llvm-mc -triple=i8085-unknown-elf < %s 2>&1 | FileCheck %s --check-prefix=NOUNDOC

        DSUB
        ARHL
        RDEL
        LDHI 0x12
        LDSI 0x34
        RSTV
        SHLX
        LHLX
        JNX5 0x1234
        JX5 0x5678
        JNK 0x2345
        JK 0x6789

# ENC: DSUB ; encoding: [0x08]
# ENC: ARHL ; encoding: [0x10]
# ENC: RDEL ; encoding: [0x18]
# ENC: LDHI 18 ; encoding: [0x28,0x12]
# ENC: LDSI 52 ; encoding: [0x38,0x34]
# ENC: RSTV ; encoding: [0xcb]
# ENC: SHLX ; encoding: [0xd9]
# ENC: LHLX ; encoding: [0xed]
# ENC: JNX5 4660 ; encoding: [0xdd,0x34,0x12]
# ENC: JX5 22136 ; encoding: [0xfd,0x78,0x56]
# ENC: JNX5 9029 ; encoding: [0xdd,0x45,0x23]
# ENC: JX5 26505 ; encoding: [0xfd,0x89,0x67]

# DISASM: 08           DSUB
# DISASM: 10           ARHL
# DISASM: 18           RDEL
# DISASM: 28 12        LDHI 0x12
# DISASM: 38 34        LDSI 0x34
# DISASM: cb           RSTV
# DISASM: d9           SHLX
# DISASM: ed           LHLX
# DISASM: dd 34 12     JNX5 0x1234
# DISASM: fd 78 56     JX5 0x5678
# DISASM: dd 45 23     JNX5 0x2345
# DISASM: fd 89 67     JX5 0x6789

# NOUNDOC: error: instruction requires a CPU feature not currently enabled
