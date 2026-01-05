//===-- TMS9900RegisterInfo.td - TMS9900 Register defs -----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// TMS9900 Register Definitions
//
// The TMS9900 has a unique "workspace" architecture where R0-R15 are actually
// 16 consecutive words in RAM, pointed to by the Workspace Pointer (WP).
// For LLVM purposes, we treat these as virtual registers and let the
// backend handle the memory access semantics.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Register Classes
//===----------------------------------------------------------------------===//

class TMS9900Reg<bits<4> num, string n> : Register<n> {
  let HWEncoding{3-0} = num;
  let Namespace = "TMS9900";
}

//===----------------------------------------------------------------------===//
// Workspace Registers (R0-R15)
// Located in memory at WP + 2*n
//===----------------------------------------------------------------------===//

def R0  : TMS9900Reg<0,  "R0">,  DwarfRegNum<[0]>;
def R1  : TMS9900Reg<1,  "R1">,  DwarfRegNum<[1]>;
def R2  : TMS9900Reg<2,  "R2">,  DwarfRegNum<[2]>;
def R3  : TMS9900Reg<3,  "R3">,  DwarfRegNum<[3]>;
def R4  : TMS9900Reg<4,  "R4">,  DwarfRegNum<[4]>;
def R5  : TMS9900Reg<5,  "R5">,  DwarfRegNum<[5]>;
def R6  : TMS9900Reg<6,  "R6">,  DwarfRegNum<[6]>;
def R7  : TMS9900Reg<7,  "R7">,  DwarfRegNum<[7]>;
def R8  : TMS9900Reg<8,  "R8">,  DwarfRegNum<[8]>;
def R9  : TMS9900Reg<9,  "R9">,  DwarfRegNum<[9]>;
def R10 : TMS9900Reg<10, "R10">, DwarfRegNum<[10]>;  // Stack Pointer (SP)
def R11 : TMS9900Reg<11, "R11">, DwarfRegNum<[11]>;  // Link Register (LR)
def R12 : TMS9900Reg<12, "R12">, DwarfRegNum<[12]>;  // CRU Base
def R13 : TMS9900Reg<13, "R13">, DwarfRegNum<[13]>;  // Old WP (context switch)
def R14 : TMS9900Reg<14, "R14">, DwarfRegNum<[14]>;  // Old PC (context switch)
def R15 : TMS9900Reg<15, "R15">, DwarfRegNum<[15]>;  // Old ST (context switch)

//===----------------------------------------------------------------------===//
// Internal Registers (not directly in workspace)
//===----------------------------------------------------------------------===//

// These are the true hardware registers inside the CPU
// We expose them for completeness but they have limited direct use

def PC : TMS9900Reg<0, "PC">, DwarfRegNum<[16]> {
  let Namespace = "TMS9900";
}

def WP : TMS9900Reg<0, "WP">, DwarfRegNum<[17]> {
  let Namespace = "TMS9900";
}

def ST : TMS9900Reg<0, "ST">, DwarfRegNum<[18]> {
  let Namespace = "TMS9900";
}

//===----------------------------------------------------------------------===//
// Register Classes
//===----------------------------------------------------------------------===//

// General purpose registers - all workspace registers usable for computation
// Allocation order prioritizes R0-R9 (caller-saved), then R13-R15 (callee-saved)
def GR16 : RegisterClass<"TMS9900", [i16], 16,
  (add R0, R1, R2, R3, R4, R5, R6, R7, R8, R9,
       R13, R14, R15,  // Callee-saved, usable with care
       R10,            // SP - usually reserved
       R11,            // LR - usually reserved
       R12)>;          // CRU base - usually reserved

// Registers usable for general allocation (excludes SP, LR, CRU)
def GPR : RegisterClass<"TMS9900", [i16], 16,
  (add R0, R1, R2, R3, R4, R5, R6, R7, R8, R9,
       R13, R14, R15)>;

// Argument/return registers
def ArgRegs : RegisterClass<"TMS9900", [i16], 16,
  (add R0, R1, R2, R3)>;

// Callee-saved registers
def CalleeSaved : RegisterClass<"TMS9900", [i16], 16,
  (add R13, R14, R15)>;

// Registers that can be used for indexing (R1-R15, not R0)
// R0 cannot be used for indexed addressing mode
def IdxRegs : RegisterClass<"TMS9900", [i16], 16,
  (add R1, R2, R3, R4, R5, R6, R7, R8, R9,
       R10, R11, R12, R13, R14, R15)>;

//===----------------------------------------------------------------------===//
// Register Pairs for 32-bit operations (MPY/DIV)
// Each pair is Rn:Rn+1 where Rn holds high word, Rn+1 holds low word
//===----------------------------------------------------------------------===//

// Define sub-register indices
def sub_lo : SubRegIndex<16>;
def sub_hi : SubRegIndex<16, 16>;

// Register pairs for MPY/DIV operations
// These are used for 32-bit results/operands
// Note: Names use colon separator to avoid conflicts with single registers
let SubRegIndices = [sub_hi, sub_lo] in {
def R0_R1   : RegisterWithSubRegs<"R0:R1", [R0, R1]>   { let Namespace = "TMS9900"; }
def R2_R3   : RegisterWithSubRegs<"R2:R3", [R2, R3]>   { let Namespace = "TMS9900"; }
def R4_R5   : RegisterWithSubRegs<"R4:R5", [R4, R5]>   { let Namespace = "TMS9900"; }
def R6_R7   : RegisterWithSubRegs<"R6:R7", [R6, R7]>   { let Namespace = "TMS9900"; }
def R8_R9   : RegisterWithSubRegs<"R8:R9", [R8, R9]>   { let Namespace = "TMS9900"; }
// R10/R11 are SP/LR, R12/R13 are CRU/OldWP - generally not used for MPY/DIV
}

// Register class for 32-bit register pairs (for MPY result, DIV dividend)
def GR32 : RegisterClass<"TMS9900", [i32], 16,
  (add R0_R1, R2_R3, R4_R5, R6_R7, R8_R9)>;

// Status register class (for flag operations)
def SR : RegisterClass<"TMS9900", [i16], 16, (add ST)> {
  let CopyCost = -1;  // Don't copy status register
  let isAllocatable = 0;
}
